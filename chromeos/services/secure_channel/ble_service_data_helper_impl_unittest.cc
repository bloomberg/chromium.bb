// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_service_data_helper_impl.h"

#include <memory>

#include "base/macros.h"
#include "components/cryptauth/remote_device_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelBleServiceDataHelperImplTest : public testing::Test {
 protected:
  SecureChannelBleServiceDataHelperImplTest() = default;
  ~SecureChannelBleServiceDataHelperImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    remote_device_cache_ =
        cryptauth::RemoteDeviceCache::Factory::Get()->BuildInstance();

    helper_ = BleServiceDataHelperImpl::Factory::Get()->BuildInstance(
        remote_device_cache_.get());
  }

 private:
  std::unique_ptr<cryptauth::RemoteDeviceCache> remote_device_cache_;

  std::unique_ptr<BleServiceDataHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleServiceDataHelperImplTest);
};

TEST_F(SecureChannelBleServiceDataHelperImplTest, Test) {
  // TODO(hansberry): Add tests once implementation is complete.
}

}  // namespace secure_channel

}  // namespace chromeos
