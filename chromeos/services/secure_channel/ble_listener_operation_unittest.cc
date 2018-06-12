// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_listener_operation.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelBleListenerOperationTest : public testing::Test {
 protected:
  SecureChannelBleListenerOperationTest() = default;
  ~SecureChannelBleListenerOperationTest() override = default;

  // testing::Test:
  void SetUp() override {}

 private:
  std::unique_ptr<BleListenerOperation> operation_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleListenerOperationTest);
};

TEST_F(SecureChannelBleListenerOperationTest, Test) {
  // TODO(khorimoto): Add tests once implementation is complete.
}

}  // namespace secure_channel

}  // namespace chromeos
