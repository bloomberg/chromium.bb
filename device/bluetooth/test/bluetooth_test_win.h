// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_

#include "device/bluetooth/test/bluetooth_test.h"

#include "base/memory/ref_counted.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"

namespace device {
class BluetoothAdapterWin;

// Windows implementation of BluetoothTestBase.
class BluetoothTestWin : public BluetoothTestBase {
 public:
  BluetoothTestWin();
  ~BluetoothTestWin() override;

  // BluetoothTestBase overrides
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  bool DenyPermission() override;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;
  BluetoothAdapterWin* adapter_win_;

  void AdapterInitCallback();
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestWin BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
