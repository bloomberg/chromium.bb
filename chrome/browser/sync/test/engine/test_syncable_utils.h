// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities that are useful in verifying the state of items in a
// syncable database.

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_SYNCABLE_UTILS_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_SYNCABLE_UTILS_H_
#pragma once

#include <string>

#include "chrome/browser/sync/syncable/syncable.h"

namespace syncable {

class BaseTransaction;
class Id;

// Count the number of entries with a given name inside of a parent.
// Useful to check folder structure and for porting older tests that
// rely on uniqueness inside of folders.
int CountEntriesWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const std::string& name);

// Get the first entry ID with name in a parent. The entry *must* exist.
Id GetFirstEntryWithName(BaseTransaction* rtrans,
                         const syncable::Id& parent_id,
                         const std::string& name);

// Assert that there's only one entry by this name in this parent.
// Return the Id.
Id GetOnlyEntryWithName(BaseTransaction* rtrans,
                        const syncable::Id& parent_id,
                        const std::string& name);

}  // namespace syncable

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_SYNCABLE_UTILS_H_
