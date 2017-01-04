// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MOCK_LOCAL_DEVICE_DATA_PROVIDER_H
#define CHROMEOS_COMPONENTS_TETHER_MOCK_LOCAL_DEVICE_DATA_PROVIDER_H

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/components/tether/local_device_data_provider.h"

namespace cryptauth {
class BeaconSeed;
}

namespace chromeos {

namespace tether {

// Test double for LocalDeviceDataProvider.
class MockLocalDeviceDataProvider : public LocalDeviceDataProvider {
 public:
  MockLocalDeviceDataProvider();
  ~MockLocalDeviceDataProvider() override;

  void SetPublicKey(std::unique_ptr<std::string> public_key);
  void SetBeaconSeeds(
      std::unique_ptr<std::vector<cryptauth::BeaconSeed>> beacon_seeds);

  // LocalDeviceDataProvider:
  bool GetLocalDeviceData(
      std::string* public_key_out,
      std::vector<cryptauth::BeaconSeed>* beacon_seeds_out) const override;

 private:
  std::unique_ptr<std::string> public_key_;
  std::unique_ptr<std::vector<cryptauth::BeaconSeed>> beacon_seeds_;

  DISALLOW_COPY_AND_ASSIGN(MockLocalDeviceDataProvider);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MOCK_LOCAL_DEVICE_DATA_PROVIDER_H
