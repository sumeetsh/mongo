/**
 *    Copyright (C) 2013 mongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

/**
 * This file contains tests for mongo/db/exec/projection_exec.cpp
 */

#include "mongo/db/exec/projection_exec.h"

#include <sstream>
#include <memory>
#include "mongo/db/json.h"
#include "mongo/db/matcher/expression_parser.h"
#include "mongo/unittest/unittest.h"

using namespace mongo;

namespace {

    using std::auto_ptr;
    using std::stringstream;

    /**
     * Utility function to create MatchExpression
     */
    MatchExpression* parseMatchExpression(const BSONObj& obj) {
        StatusWithMatchExpression status = MatchExpressionParser::parse(obj);
        ASSERT_TRUE(status.isOK());
        MatchExpression* expr(status.getValue());
        return expr;
    }

    //
    // transform tests
    //

    /**
     * test function to verify results of transform()
     * on a working set member.
     *
     * specStr - projection specification
     * queryStr - query
     * objStr - object to run projection on
     * expectedStatusOK - expected status of transformation
     * expectedObjStr - expected object after successful projection.
     *                  Ignored if expectedStatusOK is false.
     */

    void testTransform(const char* specStr, const char* queryStr, const char* objStr,
                       bool expectedStatusOK, const char* expectedObjStr) {
        // Create projection exec object.
        BSONObj spec = fromjson(specStr);
        BSONObj query = fromjson(queryStr);
        auto_ptr<MatchExpression> queryExpression(parseMatchExpression(query));
        ProjectionExec exec(spec, queryExpression.get());

        // Create working set member.
        WorkingSetMember wsm;
        wsm.state = WorkingSetMember::OWNED_OBJ;
        wsm.obj = fromjson(objStr);

        // Transform object
        Status status = exec.transform(&wsm);

        // There are fewer checks to perform if we are expected a failed status.
        if (!expectedStatusOK) {
            if (status.isOK()) {
                stringstream ss;
                ss << "expected transform() to fail but got success instead."
                   << "\nprojection spec: " << specStr
                   << "\nquery: " << queryStr
                   << "\nobject before projection: " << objStr;
                FAIL(ss.str());
            }
            return;
        }

        // If we are expecting a successful transformation but got a failed status instead,
        // print out status message in assertion message.
        if (!status.isOK()) {
            stringstream ss;
            ss << "transform() test failed: unexpected failed status: " << status.toString()
               << "\nprojection spec: " << specStr
               << "\nquery: " << queryStr
               << "\nobject before projection: " << objStr
               << "\nexpected object after projection: " << expectedObjStr;
            FAIL(ss.str());
        }

        // Finally, we compare the projected object.
        const BSONObj& obj = wsm.obj;
        BSONObj expectedObj = fromjson(expectedObjStr);
        if (obj != expectedObj) {
            stringstream ss;
            ss << "transform() test failed: unexpected projected object."
               << "\nprojection spec: " << specStr
               << "\nquery: " << queryStr
               << "\nobject before projection: " << objStr
               << "\nexpected object after projection: " << expectedObjStr
               << "\nactual object after projection: " << obj.toString();
            FAIL(ss.str());
        }
    }

    TEST(ProjectionExecTest, TransformPositionalDollar) {
        // Valid position $ projections.
        testTransform("{'a.$': 1}", "{a: 10}", "{a: [10, 20, 30]}", true, "{a: [10]}");
        testTransform("{'a.$': 1}", "{a: 20}", "{a: [10, 20, 30]}", true, "{a: [20]}");
        testTransform("{'a.$': 1}", "{a: 30}", "{a: [10, 20, 30]}", true, "{a: [30]}");
        testTransform("{'a.$': 1}", "{a: {$gt: 4}}", "{a: [5]}", true, "{a: [5]}");

        // Invalid position $ projections.
        testTransform("{'a.$': 1}", "{a: {$size: 1}}", "{a: [5]}", false, "");
    }

}  // namespace
