// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_SET_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_SET_H_

#include <set>

namespace sync_file_system {
namespace drive_backend {

class FileTracker;

class TrackerSet {
 public:
  struct TrackerComparator {
    bool operator()(const FileTracker* left,
                    const FileTracker* right) const;
  };

  typedef std::set<FileTracker*, TrackerComparator> RawTrackerSet;
  typedef RawTrackerSet::iterator iterator;
  typedef RawTrackerSet::const_iterator const_iterator;

  TrackerSet();
  ~TrackerSet();

  bool has_active() const { return !!active_tracker_; }
  void Insert(FileTracker* tracker);
  void Erase(FileTracker* tracker);
  void Activate(FileTracker* tracker);
  void Inactivate(FileTracker* tracker);

  const FileTracker* active_tracker() const { return active_tracker_; }
  FileTracker* active_tracker() { return active_tracker_; }
  const RawTrackerSet& tracker_set() const { return tracker_set_; }

  iterator begin() { return tracker_set_.begin(); }
  iterator end() { return tracker_set_.end(); }
  const_iterator begin() const { return tracker_set_.begin(); }
  const_iterator end() const { return tracker_set_.end(); }
  bool empty() const { return tracker_set_.empty(); }

 private:
  FileTracker* active_tracker_;
  RawTrackerSet tracker_set_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_TRACKER_SET_H_
