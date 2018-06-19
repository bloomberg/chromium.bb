// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/pending_connection_manager_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/secure_channel/fake_pending_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelPendingConnectionManagerImplTest : public testing::Test {
 protected:
  SecureChannelPendingConnectionManagerImplTest() = default;
  ~SecureChannelPendingConnectionManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_delegate_ = std::make_unique<FakePendingConnectionManagerDelegate>();

    manager_ = PendingConnectionManagerImpl::Factory::Get()->BuildInstance(
        fake_delegate_.get());
  }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<FakePendingConnectionManagerDelegate> fake_delegate_;

  std::unique_ptr<PendingConnectionManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelPendingConnectionManagerImplTest);
};

TEST_F(SecureChannelPendingConnectionManagerImplTest, Test) {
  // TODO(khorimoto): Add tests once implementation is complete.
}

}  // namespace secure_channel

}  // namespace chromeos
