  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
#pragma once

#include <vector>

#include "chrome/browser/sync/api/sync_error.h"

class SyncChange;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

typedef std::vector<SyncChange> SyncChangeList;

// An interface for services that handle receiving SyncChanges.
class SyncChangeProcessor {
 public:
  // Process a list of SyncChanges.
  // Returns: A default SyncError (IsSet() == false) if no errors were
  //          encountered, and a filled SyncError (IsSet() == true)
  //          otherwise.
  // Inputs:
  //   |from_here|: allows tracking of where sync changes originate.
  //   |change_list|: is the list of sync changes in need of processing.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) = 0;
 protected:
  virtual ~SyncChangeProcessor();
};


#endif  // CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
