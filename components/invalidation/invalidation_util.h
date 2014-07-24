// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_UTIL_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_UTIL_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/invalidation/invalidation_export.h"

namespace base {
class DictionaryValue;
}  // namespace

namespace invalidation {
class Invalidation;
class ObjectId;
}  // namespace invalidation

namespace syncer {

class Invalidation;

struct INVALIDATION_EXPORT ObjectIdLessThan {
  bool operator()(const invalidation::ObjectId& lhs,
                  const invalidation::ObjectId& rhs) const;
};

struct INVALIDATION_EXPORT InvalidationVersionLessThan {
  bool operator()(const syncer::Invalidation& a,
                  const syncer::Invalidation& b) const;
};

typedef std::set<invalidation::ObjectId, ObjectIdLessThan> ObjectIdSet;

typedef std::map<invalidation::ObjectId, int, ObjectIdLessThan>
    ObjectIdCountMap;

// Caller owns the returned DictionaryValue.
scoped_ptr<base::DictionaryValue> ObjectIdToValue(
    const invalidation::ObjectId& object_id);

bool ObjectIdFromValue(const base::DictionaryValue& value,
                       invalidation::ObjectId* out);

INVALIDATION_EXPORT std::string ObjectIdToString(
    const invalidation::ObjectId& object_id);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_UTIL_H_
