// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_
#define CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_
#pragma once

namespace syncable {

template <typename FieldType, FieldType field_index>
class LessField {
 public:
  inline bool operator() (const syncable::EntryKernel* a,
                          const syncable::EntryKernel* b) const {
    return a->ref(field_index) < b->ref(field_index);
  }
};

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_SYNCABLE_SYNCABLE_INL_H_
