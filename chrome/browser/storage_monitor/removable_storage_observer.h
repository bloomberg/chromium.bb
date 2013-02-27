// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_OBSERVER_H_
#define CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_OBSERVER_H_

#include "chrome/browser/storage_monitor/storage_monitor.h"

namespace chrome {

// Clients use this class to register for event-based notifications of
// removable storage devices attached to or removed from the system.
class RemovableStorageObserver {
 public:
  // When a removable storage device is attached, this
  // event is triggered.
  virtual void OnRemovableStorageAttached(
      const StorageMonitor::StorageInfo& info) {}

  // When a removable storage device is detached, this
  // event is triggered.
  virtual void OnRemovableStorageDetached(
      const StorageMonitor::StorageInfo& info) {}

 protected:
  virtual ~RemovableStorageObserver() {}
};

}  // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_REMOVABLE_STORAGE_OBSERVER_H_
