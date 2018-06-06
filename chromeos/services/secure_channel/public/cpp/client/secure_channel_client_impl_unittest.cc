// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelClientImplTest : public testing::Test {
 protected:
  SecureChannelClientImplTest() = default;

  // testing::Test:
  void SetUp() override {
    client_ = SecureChannelClientImpl::Factory::Get()->BuildInstance();
  }

  std::unique_ptr<SecureChannelClient> client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelClientImplTest);
};

TEST_F(SecureChannelClientImplTest, Test) {
  // TODO(hansberry): Implement.
}

}  // namespace secure_channel

}  // namespace chromeos
