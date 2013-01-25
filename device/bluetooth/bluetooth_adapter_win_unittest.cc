// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAdapterAddress[] = "Bluetooth Adapter Address";
const char kAdapterName[] = "Bluetooth Adapter Name";

}  // namespace

namespace device {

class BluetoothAdapterWinTest : public testing::Test {
 public:
  BluetoothAdapterWinTest()
      : adapter_(new BluetoothAdapterWin(
          base::Bind(&BluetoothAdapterWinTest::RunInitCallback,
                     base::Unretained(this)))),
        adapter_win_(static_cast<BluetoothAdapterWin*>(adapter_.get())),
        init_callback_called_(false) {
  }

  void RunInitCallback() {
    init_callback_called_ = true;
  }

 protected:
  scoped_refptr<BluetoothAdapter> adapter_;
  BluetoothAdapterWin* adapter_win_;
  bool init_callback_called_;
};

TEST_F(BluetoothAdapterWinTest, AdapterNotPresent) {
  BluetoothTaskManagerWin::AdapterState state;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_FALSE(adapter_win_->IsPresent());
}

TEST_F(BluetoothAdapterWinTest, AdapterPresent) {
  BluetoothTaskManagerWin::AdapterState state;
  state.address = kAdapterAddress;
  state.name = kAdapterName;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_TRUE(adapter_win_->IsPresent());
}

TEST_F(BluetoothAdapterWinTest, AdapterInitialized) {
  EXPECT_FALSE(adapter_win_->IsInitialized());
  EXPECT_FALSE(init_callback_called_);
  BluetoothTaskManagerWin::AdapterState state;
  adapter_win_->AdapterStateChanged(state);
  EXPECT_TRUE(adapter_win_->IsInitialized());
  EXPECT_TRUE(init_callback_called_);
}

}  // namespace device