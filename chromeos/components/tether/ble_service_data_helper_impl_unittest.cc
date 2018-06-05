// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_service_data_helper_impl.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "components/cryptauth/mock_local_device_data_provider.h"
#include "components/cryptauth/mock_remote_beacon_seed_fetcher.h"
#include "components/cryptauth/remote_device_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

class BleServiceDataHelperImplTest : public testing::Test {
 protected:
  BleServiceDataHelperImplTest() = default;
  ~BleServiceDataHelperImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_tether_host_fetcher_ = std::make_unique<FakeTetherHostFetcher>();
    mock_local_data_provider_ =
        std::make_unique<cryptauth::MockLocalDeviceDataProvider>();
    mock_seed_fetcher_ =
        std::make_unique<cryptauth::MockRemoteBeaconSeedFetcher>();

    helper_ = BleServiceDataHelperImpl::Factory::Get()->BuildInstance(
        fake_tether_host_fetcher_.get(), mock_local_data_provider_.get(),
        mock_seed_fetcher_.get());
  }

 private:
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<cryptauth::MockLocalDeviceDataProvider>
      mock_local_data_provider_;
  std::unique_ptr<cryptauth::MockRemoteBeaconSeedFetcher> mock_seed_fetcher_;

  std::unique_ptr<secure_channel::BleServiceDataHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(BleServiceDataHelperImplTest);
};

TEST_F(BleServiceDataHelperImplTest, Test) {
  // TODO(hansberry): Add tests for this class.
}

}  // namespace tether

}  // namespace chromeos
