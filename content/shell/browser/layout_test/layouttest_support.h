// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUTTEST_SUPPORT_H_

#include "base/callback_forward.h"

namespace content {
class StoragePartition;

// Turn the browser process into layout test mode.
void EnableBrowserLayoutTestMode();

// Terminates all workers and notifies when complete. This is used for
// testing when it is important to make sure that all shared worker activity
// has stopped.
void TerminateAllSharedWorkersForTesting(StoragePartition* storage_partition,
                                         base::OnceClosure callback);

// Sets the scan duration to reflect the given setting.
enum class BluetoothTestScanDurationSetting {
  kImmediateTimeout,  // Set the scan duration to 0 seconds.
  kNeverTimeout,  // Set the scan duration to base::TimeDelta::Max() seconds.
};

void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting);

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUTTEST_SUPPORT_H_
