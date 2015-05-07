// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapterAndroidTest : public testing::Test {
 protected:
  BluetoothAdapterAndroidTest() {
    adapter_ = BluetoothAdapterAndroid::CreateAdapter().get();
  }

  scoped_refptr<BluetoothAdapterAndroid> adapter_;
};

TEST_F(BluetoothAdapterAndroidTest, Construct) {
  ASSERT_TRUE(adapter_.get());
  EXPECT_FALSE(adapter_->HasBluetoothPermission());
}

}  // namespace device
