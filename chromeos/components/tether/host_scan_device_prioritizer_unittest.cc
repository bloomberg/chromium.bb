// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_device_prioritizer.h"

#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

class HostScanDevicePrioritizerTest : public testing::Test {
 protected:
  HostScanDevicePrioritizerTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(10)) {}

  void SetUp() override {
    pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    HostScanDevicePrioritizer::RegisterPrefs(pref_service_->registry());

    prioritizer_ =
        base::MakeUnique<HostScanDevicePrioritizer>(pref_service_.get());
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<HostScanDevicePrioritizer> prioritizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScanDevicePrioritizerTest);
};

TEST_F(HostScanDevicePrioritizerTest, TestOnlyTetherAvailabilityResponses) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Do not receive a ConnectTetheringResponse.

  std::vector<cryptauth::RemoteDevice> test_vector =
      std::vector<cryptauth::RemoteDevice>{test_devices_[6], test_devices_[5],
                                           test_devices_[4], test_devices_[3],
                                           test_devices_[2], test_devices_[1],
                                           test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((std::vector<cryptauth::RemoteDevice>{
                test_devices_[4], test_devices_[3], test_devices_[2],
                test_devices_[1], test_devices_[0], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerTest, TestBothTypesOfResponses) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Receive ConnectTetheringResponse from device 0.
  prioritizer_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);

  std::vector<cryptauth::RemoteDevice> test_vector =
      std::vector<cryptauth::RemoteDevice>{test_devices_[6], test_devices_[5],
                                           test_devices_[4], test_devices_[3],
                                           test_devices_[2], test_devices_[1],
                                           test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((std::vector<cryptauth::RemoteDevice>{
                test_devices_[0], test_devices_[4], test_devices_[3],
                test_devices_[2], test_devices_[1], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

TEST_F(HostScanDevicePrioritizerTest, TestBothTypesOfResponses_DifferentOrder) {
  // Receive different order.
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  prioritizer_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive ConnectTetheringResponse from device 1.
  prioritizer_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);

  std::vector<cryptauth::RemoteDevice> test_vector =
      std::vector<cryptauth::RemoteDevice>{test_devices_[9], test_devices_[8],
                                           test_devices_[7], test_devices_[6],
                                           test_devices_[5], test_devices_[4],
                                           test_devices_[3], test_devices_[2],
                                           test_devices_[1], test_devices_[0]};

  prioritizer_->SortByHostScanOrder(&test_vector);
  EXPECT_EQ((std::vector<cryptauth::RemoteDevice>{
                test_devices_[1], test_devices_[3], test_devices_[4],
                test_devices_[2], test_devices_[0], test_devices_[9],
                test_devices_[8], test_devices_[7], test_devices_[6],
                test_devices_[5]}),
            test_vector);
}

}  // namespace tether

}  // namespace cryptauth
