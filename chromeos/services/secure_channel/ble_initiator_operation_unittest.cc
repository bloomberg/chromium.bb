// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_operation.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelBleInitiatorOperationTest : public testing::Test {
 protected:
  SecureChannelBleInitiatorOperationTest() = default;
  ~SecureChannelBleInitiatorOperationTest() override = default;

  // testing::Test:
  void SetUp() override {}

 private:
  std::unique_ptr<BleInitiatorOperation> operation_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleInitiatorOperationTest);
};

TEST_F(SecureChannelBleInitiatorOperationTest, Test) {
  // TODO(khorimoto): Add tests once implementation is complete.
}

}  // namespace secure_channel

}  // namespace chromeos
