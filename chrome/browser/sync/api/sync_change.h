// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_API_SYNC_CHANGE_H_
#define CHROME_BROWSER_SYNC_API_SYNC_CHANGE_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/sync/api/sync_data.h"

// A SyncChange object reflects a change to a piece of synced data. The change
// can be either a delete, add, or an update. All data relevant to the change
// is encapsulated within the SyncChange, which, once created, is immutable.
// Note: it is safe and cheap to pass these by value or make copies, as they do
// not create deep copies of their internal data.
class SyncChange {
 public:
  enum SyncChangeType {
    ACTION_INVALID,
    ACTION_ADD,
    ACTION_UPDATE,
    ACTION_DELETE
  };

  // Default constructor creates an invalid change.
  SyncChange();
  // Create a new change with the specified sync data.
  SyncChange(SyncChangeType change_type, const SyncData& sync_data);
  ~SyncChange();

  // Copy constructor and assignment operator welcome.

  // Whether this change is valid. This must be true before attempting to access
  // the data.
  // Deletes: Requires valid tag when going to the syncer. Requires valid
  //          specifics when coming from the syncer.
  // Adds, Updates: Require valid tag and specifics when going to the syncer.
  //                Require only valid specifics when coming from the syncer.
  bool IsValid() const;

  // Getters.
  SyncChangeType change_type() const;
  SyncData sync_data() const;

 private:
  SyncChangeType change_type_;

  // An immutable container for the data of this SyncChange. Whenever
  // SyncChanges are copied, they copy references to this data.
  SyncData sync_data_;
};

#endif  // CHROME_BROWSER_SYNC_API_SYNC_CHANGE_H_
