// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYSTEM_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_
#define CHROME_BROWSER_SYSTEM_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_

#include "chrome/browser/system_monitor/removable_storage_notifications.h"
#include "chrome/browser/system_monitor/removable_storage_observer.h"

namespace chrome {

class MockRemovableStorageObserver : public RemovableStorageObserver {
 public:
  MockRemovableStorageObserver();
  virtual ~MockRemovableStorageObserver();

  virtual void OnRemovableStorageAttached(
      const RemovableStorageNotifications::StorageInfo& info) OVERRIDE;

  virtual void OnRemovableStorageDetached(
      const RemovableStorageNotifications::StorageInfo& info) OVERRIDE;

  int attach_calls() { return attach_calls_; }

  int detach_calls() { return detach_calls_; }

  const RemovableStorageNotifications::StorageInfo& last_attached() {
    return last_attached_;
  }

  const RemovableStorageNotifications::StorageInfo& last_detached() {
    return last_detached_;
  }

 private:
  int attach_calls_;
  int detach_calls_;
  RemovableStorageNotifications::StorageInfo last_attached_;
  RemovableStorageNotifications::StorageInfo last_detached_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_SYSTEM_MONITOR_MOCK_REMOVABLE_STORAGE_OBSERVER_H_
