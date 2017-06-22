// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/host_scan_device_prioritizer_impl.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "chromeos/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/components/tether/tether_host_response_recorder.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

class HostScanDevicePrioritizerImplTest : public NetworkStateTest {
 protected:
  HostScanDevicePrioritizerImplTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(10)) {}

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkStateTest::SetUp();

    network_state_handler()->SetTetherTechnologyState(
        NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED);

    pref_service_ = base::MakeUnique<TestingPrefServiceSimple>();
    TetherHostResponseRecorder::RegisterPrefs(pref_service_->registry());

    recorder_ =
        base::MakeUnique<TetherHostResponseRecorder>(pref_service_.get());
    device_id_tether_network_guid_map_ =
        base::MakeUnique<DeviceIdTetherNetworkGuidMap>();

    prioritizer_ = base::MakeUnique<HostScanDevicePrioritizerImpl>(
        recorder_.get(), device_id_tether_network_guid_map_.get());
    network_state_handler()->set_tether_sort_delegate(prioritizer_.get());
  }

  void TearDown() override {
    prioritizer_.reset();
    ShutdownNetworkState();
    NetworkStateTest::TearDown();
    DBusThreadManager::Shutdown();
  }

  void VerifyNetworksInExpectedOrder(
      std::vector<cryptauth::RemoteDevice> expected) {
    NetworkStateHandler::NetworkStateList network_list;

    network_state_handler()->GetVisibleNetworkListByType(
        NetworkTypePattern::Tether(), &network_list);
    EXPECT_EQ(expected.size(), network_list.size());

    for (size_t i = 0; i < expected.size(); i++) {
      std::string expected_guid =
          device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
              expected[i].GetDeviceId());
      EXPECT_EQ(expected_guid, network_list[i]->guid());
    }
  }

  void AddNetworkStateForDevice(const cryptauth::RemoteDevice& remote_device) {
    std::string guid =
        device_id_tether_network_guid_map_->GetTetherNetworkGuidForDeviceId(
            remote_device.GetDeviceId());
    network_state_handler()->AddTetherNetworkState(
        guid, "name", "carrier", 100 /* battery_percentage */,
        100 /* signal_strength */, false /* has_connected_to_host */);
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TetherHostResponseRecorder> recorder_;
  // TODO(hansberry): Use a fake for this when a real mapping scheme is created.
  std::unique_ptr<DeviceIdTetherNetworkGuidMap>
      device_id_tether_network_guid_map_;

  std::unique_ptr<HostScanDevicePrioritizerImpl> prioritizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostScanDevicePrioritizerImplTest);
};

TEST_F(HostScanDevicePrioritizerImplTest,
       TestOnlyTetherAvailabilityResponses_RemoteDevices) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

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

TEST_F(HostScanDevicePrioritizerImplTest,
       TestOnlyTetherAvailabilityResponses_NetworkStates) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Do not receive a ConnectTetheringResponse.

  AddNetworkStateForDevice(test_devices_[6]);
  AddNetworkStateForDevice(test_devices_[5]);
  AddNetworkStateForDevice(test_devices_[4]);
  AddNetworkStateForDevice(test_devices_[3]);
  AddNetworkStateForDevice(test_devices_[2]);
  AddNetworkStateForDevice(test_devices_[1]);
  AddNetworkStateForDevice(test_devices_[0]);

  VerifyNetworksInExpectedOrder((std::vector<cryptauth::RemoteDevice>{
      test_devices_[4], test_devices_[3], test_devices_[2], test_devices_[1],
      test_devices_[0], test_devices_[6], test_devices_[5]}));
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_RemoteDevices) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Receive ConnectTetheringResponse from device 0.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);

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

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_NetworkStates) {
  // Receive TetherAvailabilityResponses from devices 0-4.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);

  // Receive ConnectTetheringResponse from device 0.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[0]);

  AddNetworkStateForDevice(test_devices_[6]);
  AddNetworkStateForDevice(test_devices_[5]);
  AddNetworkStateForDevice(test_devices_[4]);
  AddNetworkStateForDevice(test_devices_[3]);
  AddNetworkStateForDevice(test_devices_[2]);
  AddNetworkStateForDevice(test_devices_[1]);
  AddNetworkStateForDevice(test_devices_[0]);

  VerifyNetworksInExpectedOrder((std::vector<cryptauth::RemoteDevice>{
      test_devices_[0], test_devices_[4], test_devices_[3], test_devices_[2],
      test_devices_[1], test_devices_[6], test_devices_[5]}));
}

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_DifferentOrder_RemoteDevices) {
  // Receive different order.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive ConnectTetheringResponse from device 1.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);

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

TEST_F(HostScanDevicePrioritizerImplTest,
       TestBothTypesOfResponses_DifferentOrder_NetworkStates) {
  // Receive different order.
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[0]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[2]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[1]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[4]);
  recorder_->RecordSuccessfulTetherAvailabilityResponse(test_devices_[3]);

  // Receive ConnectTetheringResponse from device 1.
  recorder_->RecordSuccessfulConnectTetheringResponse(test_devices_[1]);

  AddNetworkStateForDevice(test_devices_[9]);
  AddNetworkStateForDevice(test_devices_[8]);
  AddNetworkStateForDevice(test_devices_[7]);
  AddNetworkStateForDevice(test_devices_[6]);
  AddNetworkStateForDevice(test_devices_[5]);
  AddNetworkStateForDevice(test_devices_[4]);
  AddNetworkStateForDevice(test_devices_[3]);
  AddNetworkStateForDevice(test_devices_[2]);
  AddNetworkStateForDevice(test_devices_[1]);
  AddNetworkStateForDevice(test_devices_[0]);

  VerifyNetworksInExpectedOrder((std::vector<cryptauth::RemoteDevice>{
      test_devices_[1], test_devices_[3], test_devices_[4], test_devices_[2],
      test_devices_[0], test_devices_[9], test_devices_[8], test_devices_[7],
      test_devices_[6], test_devices_[5]}));
}

}  // namespace tether

}  // namespace chromeos
