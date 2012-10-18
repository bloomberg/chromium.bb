// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_OBSERVER_H_

namespace drive {

// Interface for classes that need to observe events from DriveSyncClient.
// All events are notified on UI thread.
class DriveSyncClientObserver {
 public:
  // Triggered when the sync client started a sync task.
  virtual void OnSyncTaskStarted() = 0;
  // Triggered when the sync client stopped.
  virtual void OnSyncClientStopped() = 0;
  // Triggered when the sync client became idle, that is, finished all tasks
  // and waiting for new tasks to come.
  virtual void OnSyncClientIdle() = 0;

 protected:
  virtual ~DriveSyncClientObserver() {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_SYNC_CLIENT_OBSERVER_H_
