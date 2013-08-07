// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/tracker_set.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"

namespace sync_file_system {
namespace drive_backend {

bool TrackerSet::TrackerComparator::operator()(const FileTracker* left,
                                               const FileTracker* right) const {
  return left->tracker_id() < right->tracker_id();
}

TrackerSet::TrackerSet()
    : active_tracker_(NULL) {
}

TrackerSet::~TrackerSet() {}

void TrackerSet::Insert(FileTracker* tracker) {
  if (tracker_set_.insert(tracker).second && tracker->active()) {
    DCHECK(!active_tracker_);
    active_tracker_ = tracker;
  }
}

void TrackerSet::Erase(FileTracker* tracker) {
  if (tracker_set_.erase(tracker) == 1 && active_tracker_ == tracker)
    active_tracker_ = NULL;
}

void TrackerSet::Activate(FileTracker* tracker) {
  DCHECK(!active_tracker_);
  DCHECK(ContainsKey(tracker_set_, tracker));
  active_tracker_ = tracker;
}

void TrackerSet::Inactivate(FileTracker* tracker) {
  DCHECK(tracker->active());
  DCHECK_EQ(tracker, active_tracker_);
  DCHECK(ContainsKey(tracker_set_, tracker));
  active_tracker_ = NULL;
}

}  // namespace drive_backend
}  // namespace sync_file_system
