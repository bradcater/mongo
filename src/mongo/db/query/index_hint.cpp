/**
 *    Copyright (C) 2023-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/db/query/index_hint.h"

#include "mongo/base/error_codes.h"
#include "mongo/bson/bsonmisc.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/bson/bsontypes.h"
#include "mongo/bson/simple_bsonobj_comparator.h"
#include "mongo/stdx/variant.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/overloaded_visitor.h"  // IWYU pragma: keep
#include "mongo/util/str.h"

namespace mongo {
namespace {
static constexpr auto kNaturalFieldName = "$natural"_sd;
};  // namespace

IndexHint IndexHint::parse(const BSONElement& element) {
    if (element.type() == BSONType::String) {
        return IndexHint(element.String());
    } else if (element.type() == BSONType::Object) {
        auto obj = element.Obj();
        if (obj.firstElementFieldName() == kNaturalFieldName) {
            switch (obj.firstElement().numberInt()) {
                case 1:
                    return IndexHint(NaturalOrderHint(NaturalOrderHint::Direction::kForward));
                case -1:
                    return IndexHint(NaturalOrderHint(NaturalOrderHint::Direction::kBackward));
                default:
                    uasserted(ErrorCodes::FailedToParse,
                              str::stream() << "$natural hint may only accept 1 or -1, not "
                                            << element.toString());
            }
        }
        return IndexHint(obj.getOwned());
    } else {
        uasserted(ErrorCodes::FailedToParse, "Hint must be a string or an object");
    }
}

void IndexHint::append(const IndexHint& hint, StringData fieldName, BSONObjBuilder* builder) {
    stdx::visit(
        OverloadedVisitor{
            [&](const IndexKeyPattern& keyPattern) { builder->append(fieldName, keyPattern); },
            [&](const IndexName& indexName) { builder->append(fieldName, indexName); },
            [&](const NaturalOrderHint& naturalOrderHint) {
                builder->append(fieldName, BSON(kNaturalFieldName << naturalOrderHint.direction));
            }},
        hint._hint);
}

void IndexHint::append(BSONArrayBuilder* builder) const {
    stdx::visit(OverloadedVisitor{
                    [&](const IndexKeyPattern& keyPattern) { builder->append(keyPattern); },
                    [&](const IndexName& indexName) { builder->append(indexName); },
                    [&](const NaturalOrderHint& naturalOrderHint) {
                        builder->append(BSON(kNaturalFieldName << naturalOrderHint.direction));
                    }},
                _hint);
}

boost::optional<const IndexKeyPattern&> IndexHint::getIndexKeyPattern() const {
    if (!stdx::holds_alternative<IndexKeyPattern>(_hint)) {
        return {};
    }
    return stdx::get<IndexKeyPattern>(_hint);
}

boost::optional<const IndexName&> IndexHint::getIndexName() const {
    if (!stdx::holds_alternative<IndexName>(_hint)) {
        return {};
    }
    return stdx::get<IndexName>(_hint);
}

boost::optional<const NaturalOrderHint&> IndexHint::getNaturalHint() const {
    if (!stdx::holds_alternative<NaturalOrderHint>(_hint)) {
        return {};
    }
    return stdx::get<NaturalOrderHint>(_hint);
}

size_t IndexHint::hash() const {
    return stdx::visit(
        OverloadedVisitor{
            [&](const IndexKeyPattern& keyPattern) {
                return SimpleBSONObjComparator::kInstance.hash(keyPattern);
            },
            [&](const IndexName& indexName) { return absl::Hash<std::string>{}(indexName); },
            [&](const NaturalOrderHint& naturalOrderHint) {
                return absl::Hash<NaturalOrderHint::Direction>{}(naturalOrderHint.direction);
            }},
        _hint);
}

};  // namespace mongo
