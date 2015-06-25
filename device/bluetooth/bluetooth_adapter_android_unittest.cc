// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "device/bluetooth/android/bluetooth_adapter_wrapper.h"
#include "device/bluetooth/bluetooth_adapter_android.h"
#include "jni/FakeBluetoothAdapter_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace device {

class BluetoothAdapterAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(AttachCurrentThread());
    ASSERT_TRUE(RegisterNativesImpl(AttachCurrentThread()));
  }

  void InitWithDefaultAdapter() {
    adapter_ =
        BluetoothAdapterAndroid::Create(
            BluetoothAdapterWrapper::CreateWithDefaultAdapter().obj()).get();
  }

  void InitWithoutDefaultAdapter() {
    adapter_ = BluetoothAdapterAndroid::Create(NULL).get();
  }

  void InitWithFakeAdapter() {
    j_fake_bluetooth_adapter_.Reset(
        Java_FakeBluetoothAdapter_create(AttachCurrentThread()));

    adapter_ =
        BluetoothAdapterAndroid::Create(j_fake_bluetooth_adapter_.obj()).get();
  }

  scoped_refptr<BluetoothAdapterAndroid> adapter_;
  base::android::ScopedJavaGlobalRef<jobject> j_fake_bluetooth_adapter_;
};

TEST_F(BluetoothAdapterAndroidTest, Construct) {
  InitWithDefaultAdapter();
  ASSERT_TRUE(adapter_.get());
  if (!adapter_->IsPresent()) {
    LOG(WARNING) << "Bluetooth adapter not present; skipping unit test.";
    return;
  }
  EXPECT_GT(adapter_->GetAddress().length(), 0u);
  EXPECT_GT(adapter_->GetName().length(), 0u);
  EXPECT_TRUE(adapter_->IsPresent());
  // Don't know on test machines if adapter will be powered or not, but
  // the call should be safe to make and consistent.
  EXPECT_EQ(adapter_->IsPowered(), adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterAndroidTest, ConstructWithoutDefaultAdapter) {
  InitWithoutDefaultAdapter();
  ASSERT_TRUE(adapter_.get());
  EXPECT_EQ(adapter_->GetAddress(), "");
  EXPECT_EQ(adapter_->GetName(), "");
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothAdapterAndroidTest, ConstructFakeAdapter) {
  InitWithFakeAdapter();
  ASSERT_TRUE(adapter_.get());
  EXPECT_EQ(adapter_->GetAddress(), "A1:B2:C3:D4:E5:F6");
  EXPECT_EQ(adapter_->GetName(), "FakeBluetoothAdapter");
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_TRUE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscoverable());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

}  // namespace device
