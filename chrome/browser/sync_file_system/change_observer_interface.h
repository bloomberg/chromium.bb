// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_CHANGE_OBSERVER_INTERFACE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_CHANGE_OBSERVER_INTERFACE_H_

#include "base/basictypes.h"

namespace sync_file_system {

class LocalChangeObserver {
 public:
  LocalChangeObserver() {}
  virtual ~LocalChangeObserver() {}

  // This is called when there're one ore more local changes are available.
  // |pending_changes_hint| indicates the pending queue length to help sync
  // scheduling but the value may not be accurately reflect the real-time
  // value.
  virtual void OnLocalChangeAvailable(int64 pending_changes_hint) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalChangeObserver);
};

class RemoteChangeObserver {
 public:
  RemoteChangeObserver() {}
  virtual ~RemoteChangeObserver() {}

  // This is called when there're one ore more remote changes are available.
  // |pending_changes_hint| indicates the pending queue length to help sync
  // scheduling but the value may not be accurately reflect the real-time
  // value.
  virtual void OnRemoteChangeAvailable(int64 pending_changes_hint) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteChangeObserver);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_CHANGE_OBSERVER_INTERFACE_H_
