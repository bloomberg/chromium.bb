  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
#pragma once

#include <vector>

class SyncChange;

typedef std::vector<SyncChange> SyncChangeList;

// An interface for services that handle receiving SyncChanges.
class SyncChangeProcessor {
 public:
  // Process a list of SyncChanges.
  virtual void ProcessSyncChanges(const SyncChangeList& change_list) = 0;
 protected:
  virtual ~SyncChangeProcessor();
};


#endif  // CHROME_BROWSER_SYNC_API_SYNC_CHANGE_PROCESSOR_H_
