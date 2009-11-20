// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities to verify the state of items in unit tests.

#include "chrome/test/sync/engine/test_syncable_utils.h"

#include "chrome/browser/sync/syncable/syncable.h"

namespace syncable {

int CountEntriesWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const PathString& name) {
  Directory::ChildHandles child_handles;
  rtrans->directory()->GetChildHandles(rtrans, parent_id, &child_handles);
  if (child_handles.size() <= 0) {
    return 0;
  }

  int number_of_entries_with_name = 0;
  for (Directory::ChildHandles::iterator i = child_handles.begin();
       i != child_handles.end(); ++i) {
    Entry e(rtrans, GET_BY_HANDLE, *i);
    CHECK(e.good());
    if (e.Get(NON_UNIQUE_NAME) == name) {
      ++number_of_entries_with_name;
    }
  }
  return number_of_entries_with_name;
}

Id GetFirstEntryWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const PathString& name) {
  Directory::ChildHandles child_handles;
  rtrans->directory()->GetChildHandles(rtrans, parent_id, &child_handles);

  for (Directory::ChildHandles::iterator i = child_handles.begin();
       i != child_handles.end(); ++i) {
    Entry e(rtrans, GET_BY_HANDLE, *i);
    CHECK(e.good());
    if (e.Get(NON_UNIQUE_NAME) == name) {
      return e.Get(ID);
    }
  }

  CHECK(false);
  return Id();
}

Id GetOnlyEntryWithName(BaseTransaction* rtrans,
                        const syncable::Id& parent_id,
                        const PathString& name) {
  CHECK(1 == CountEntriesWithName(rtrans, parent_id, name));
  return GetFirstEntryWithName(rtrans, parent_id, name);
}

}  // namespace syncable
