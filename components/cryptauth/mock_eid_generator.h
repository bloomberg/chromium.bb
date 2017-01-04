// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_BLE_MOCK_EID_GENERATOR_H_
#define COMPONENTS_CRYPTAUTH_BLE_MOCK_EID_GENERATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/cryptauth/eid_generator.h"

namespace cryptauth {

class BeaconSeed;
struct RemoteDevice;

// Mock class for EidGenerator. Note that GoogleMock cannot be used to mock this
// class because GoogleMock's mock functions cannot return a |std::unique_ptr|.
class MockEidGenerator : public EidGenerator {
 public:
  MockEidGenerator();
  ~MockEidGenerator() override;

  // Setters for the return values of the overridden functions below.
  void set_background_scan_filter(
      std::unique_ptr<EidData> background_scan_filter) {
    background_scan_filter_ = std::move(background_scan_filter);
  }

  void set_advertisement(std::unique_ptr<DataWithTimestamp> advertisement) {
    advertisement_ = std::move(advertisement);
  }

  void set_possible_advertisements(
      std::unique_ptr<std::vector<std::string>> possible_advertisements) {
    possible_advertisements_ = std::move(possible_advertisements);
  }

  void set_identified_device(RemoteDevice* identified_device) {
    identified_device_ = identified_device;
  }

  // EidGenerator:
  std::unique_ptr<EidData> GenerateBackgroundScanFilter(
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds)
          const override;
  std::unique_ptr<DataWithTimestamp> GenerateAdvertisement(
      const std::string& advertising_device_public_key,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds)
          const override;
  std::vector<std::string> GeneratePossibleAdvertisements(
      const std::string& advertising_device_public_key,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds)
          const override;
  RemoteDevice const* IdentifyRemoteDeviceByAdvertisement(
      const std::string& advertisement_service_data,
      const std::vector<RemoteDevice>& device_list,
      const std::vector<BeaconSeed>& scanning_device_beacon_seeds)
          const override;

  int num_identify_calls() {
    return num_identify_calls_;
  }

 private:
  std::unique_ptr<EidData> background_scan_filter_;
  std::unique_ptr<DataWithTimestamp> advertisement_;
  std::unique_ptr<std::vector<std::string>> possible_advertisements_;
  const RemoteDevice* identified_device_;

  int num_identify_calls_;
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLE_MOCK_EID_GENERATOR_H_
