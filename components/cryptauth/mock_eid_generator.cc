// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/mock_eid_generator.h"

#include "base/memory/ptr_util.h"

namespace cryptauth {

MockEidGenerator::MockEidGenerator() : background_scan_filter_(nullptr),
                                       advertisement_(nullptr),
                                       possible_advertisements_(nullptr),
                                       identified_device_(nullptr),
                                       num_identify_calls_(0) {}

MockEidGenerator::~MockEidGenerator() {}

std::unique_ptr<EidGenerator::EidData>
MockEidGenerator::GenerateBackgroundScanFilter(
    const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const {
  if (!background_scan_filter_) {
    return nullptr;
  }

  std::unique_ptr<EidGenerator::DataWithTimestamp> adjacent_data;
  if (background_scan_filter_->adjacent_data) {
    adjacent_data = base::MakeUnique<DataWithTimestamp>(
        background_scan_filter_->adjacent_data->data,
        background_scan_filter_->adjacent_data->start_timestamp_ms,
        background_scan_filter_->adjacent_data->end_timestamp_ms);
  }

  return base::MakeUnique<EidData>(
      background_scan_filter_->current_data, std::move(adjacent_data));
}

std::unique_ptr<EidGenerator::DataWithTimestamp>
MockEidGenerator::GenerateAdvertisement(
    const std::string& advertising_device_public_key,
    const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const {
  if (!advertisement_) {
    return nullptr;
  }

  return base::MakeUnique<DataWithTimestamp>(
      advertisement_->data,
      advertisement_->start_timestamp_ms,
      advertisement_->end_timestamp_ms);
}

std::vector<std::string> MockEidGenerator::GeneratePossibleAdvertisements(
    const std::string& advertising_device_public_key,
    const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const {
  if (!possible_advertisements_) {
    return std::vector<std::string>();
  }

  return *possible_advertisements_;
}

RemoteDevice const* MockEidGenerator::IdentifyRemoteDeviceByAdvertisement(
    const std::string& advertisement_service_data,
    const std::vector<RemoteDevice>& device_list,
    const std::vector<BeaconSeed>& scanning_device_beacon_seeds) const {
  // Increment num_identify_calls_. Since this overrides a const method, some
  // hacking is needed to modify the num_identify_calls_ instance variable.
  int* num_identify_calls_ptr = const_cast<int*>(&num_identify_calls_);
  *num_identify_calls_ptr = *num_identify_calls_ptr + 1;

  return identified_device_;
}

}  // namespace cryptauth
