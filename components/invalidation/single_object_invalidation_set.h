// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_SINGLE_OBJECT_INVALIDATION_SET_H_
#define COMPONENTS_INVALIDATION_SINGLE_OBJECT_INVALIDATION_SET_H_

#include <set>

#include "base/memory/scoped_ptr.h"
#include "components/invalidation/invalidation_export.h"
#include "sync/internal_api/public/base/invalidation.h"
#include "sync/internal_api/public/base/invalidation_util.h"

namespace base {
class ListValue;
}  // namespace base

namespace syncer {

// Holds a list of invalidations that all share the same Object ID.
//
// The list is kept sorted by version to make it easier to perform common
// operations, like checking for an unknown version invalidation or fetching the
// highest invalidation with the highest version number.
class INVALIDATION_EXPORT SingleObjectInvalidationSet {
 public:
  typedef std::set<Invalidation, InvalidationVersionLessThan> InvalidationsSet;
  typedef InvalidationsSet::const_iterator const_iterator;
  typedef InvalidationsSet::const_reverse_iterator const_reverse_iterator;

  SingleObjectInvalidationSet();
  ~SingleObjectInvalidationSet();

  void Insert(const Invalidation& invalidation);
  void InsertAll(const SingleObjectInvalidationSet& other);
  void Clear();
  void Erase(const_iterator it);

  // Returns true if this list contains an unknown version.
  //
  // Unknown version invalidations always end up at the start of the list,
  // because they have the lowest possible value in the sort ordering.
  bool StartsWithUnknownVersion() const;
  size_t GetSize() const;
  bool IsEmpty() const;
  bool operator==(const SingleObjectInvalidationSet& other) const;

  const_iterator begin() const;
  const_iterator end() const;
  const_reverse_iterator rbegin() const;
  const_reverse_iterator rend() const;
  const Invalidation& back() const;

  scoped_ptr<base::ListValue> ToValue() const;
  bool ResetFromValue(const base::ListValue& list);

 private:
  InvalidationsSet invalidations_;
};

}  // syncer

#endif  // COMPONENTS_INVALIDATION_SINGLE_OBJECT_INVALIDATION_SET_H_
