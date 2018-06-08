// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_advertiser_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {}  // namespace

class SecureChannelBleAdvertiserImplTest : public testing::Test {
 protected:
  SecureChannelBleAdvertiserImplTest() {}
  ~SecureChannelBleAdvertiserImplTest() override = default;

  // testing::Test:
  void SetUp() override {}

 private:
 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleAdvertiserImplTest);
};

TEST_F(SecureChannelBleAdvertiserImplTest, UrelatedScanResults) {}

}  // namespace secure_channel

}  // namespace chromeos
