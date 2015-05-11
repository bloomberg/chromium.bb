// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapterAndroidTest : public testing::Test {
 protected:
  void InitWithPermission() {
    adapter_ = BluetoothAdapterAndroid::CreateAdapter().get();
  }

  void InitWithoutPermission() {
    adapter_ =
        BluetoothAdapterAndroid::CreateAdapterWithoutPermissionForTesting()
            .get();
  }

  scoped_refptr<BluetoothAdapterAndroid> adapter_;
};

TEST_F(BluetoothAdapterAndroidTest, Construct) {
  InitWithPermission();
  ASSERT_TRUE(adapter_.get());
  EXPECT_TRUE(adapter_->HasBluetoothPermission());
  if (!adapter_->IsPresent()) {
    LOG(WARNING) << "Bluetooth adapter not present; skipping unit test.";
    return;
  }
  EXPECT_GT(adapter_->GetAddress().length(), 0u);
  EXPECT_GT(adapter_->GetName().length(), 0u);
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterAndroidTest, ConstructNoPermision) {
  InitWithoutPermission();
  ASSERT_TRUE(adapter_.get());
  EXPECT_FALSE(adapter_->HasBluetoothPermission());
  EXPECT_EQ(adapter_->GetAddress().length(), 0u);
  EXPECT_EQ(adapter_->GetName().length(), 0u);
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

}  // namespace device
