// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/mock_removable_storage_observer.h"

namespace chrome {

MockRemovableStorageObserver::MockRemovableStorageObserver()
    : attach_calls_(0), detach_calls_(0) {
}

MockRemovableStorageObserver::~MockRemovableStorageObserver() {
}

void MockRemovableStorageObserver::OnRemovableStorageAttached(
    const StorageMonitor::StorageInfo& info) {
  attach_calls_++;
  last_attached_ = info;
}

void MockRemovableStorageObserver::OnRemovableStorageDetached(
    const StorageMonitor::StorageInfo& info) {
  detach_calls_++;
  last_detached_ = info;
}

}  // namespace chrome
