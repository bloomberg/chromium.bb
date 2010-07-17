// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_

#include "chrome/common/sqlite_utils.h"

namespace syncable {

template <typename FieldType, FieldType field_index>
class LessField {
 public:
  inline bool operator() (const syncable::EntryKernel* a,
                          const syncable::EntryKernel* b) const {
    return a->ref(field_index) < b->ref(field_index);
  }
};

struct IdRowTraits {
  typedef syncable::Id RowType;
  void Extract(SQLStatement* statement, syncable::Id* id) const {
    id->s_ = statement->column_string(0);
  }
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_
