// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_scanner_impl.h"

#include "base/callback_forward.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/fake_ble_synchronizer.h"
#include "components/cryptauth/mock_foreground_eid_generator.h"
#include "components/cryptauth/mock_local_device_data_provider.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {

class TestBleScannerObserver final : public BleScanner::Observer {
 public:
  TestBleScannerObserver() {}

  std::vector<std::string>& device_addresses() { return device_addresses_; }

  std::vector<cryptauth::RemoteDevice>& remote_devices() {
    return remote_devices_;
  }

  std::vector<bool>& discovery_session_state_changes() {
    return discovery_session_state_changes_;
  }

  // BleScanner::Observer:
  void OnReceivedAdvertisementFromDevice(
      const cryptauth::RemoteDevice& remote_device,
      device::BluetoothDevice* bluetooth_device) override {
    device_addresses_.push_back(bluetooth_device->GetAddress());
    remote_devices_.push_back(remote_device);
  }

  void OnDiscoverySessionStateChanged(bool discovery_session_active) override {
    discovery_session_state_changes_.push_back(discovery_session_active);
  }

 private:
  std::vector<std::string> device_addresses_;
  std::vector<cryptauth::RemoteDevice> remote_devices_;
  std::vector<bool> discovery_session_state_changes_;
};

// Deletes the BleScanner when notified.
class DeletingObserver final : public BleScanner::Observer {
 public:
  DeletingObserver(std::unique_ptr<BleScannerImpl>& ble_scanner)
      : ble_scanner_(ble_scanner) {
    ble_scanner_->AddObserver(this);
  }

  // BleScanner::Observer:
  void OnDiscoverySessionStateChanged(bool discovery_session_active) override {
    // Only delete if the discovery session is no longer active.
    if (discovery_session_active)
      return;

    ble_scanner_->RemoveObserver(this);
    ble_scanner_.reset();
  }

  void OnReceivedAdvertisementFromDevice(
      const cryptauth::RemoteDevice& remote_device,
      device::BluetoothDevice* bluetooth_device) override {}

 private:
  std::unique_ptr<BleScannerImpl>& ble_scanner_;
};

class MockBluetoothDeviceWithServiceData : public device::MockBluetoothDevice {
 public:
  MockBluetoothDeviceWithServiceData(device::MockBluetoothAdapter* adapter,
                                     const std::string& device_address,
                                     const std::string& service_data)
      : device::MockBluetoothDevice(adapter,
                                    /* bluetooth_class */ 0,
                                    "name",
                                    device_address,
                                    false,
                                    false) {
    for (size_t i = 0; i < service_data.size(); i++) {
      service_data_.push_back(static_cast<uint8_t>(service_data[i]));
    }
  }

  const std::vector<uint8_t>* service_data() { return &service_data_; }

 private:
  std::vector<uint8_t> service_data_;
};

const size_t kMinNumBytesInServiceData = 4;

const char kDefaultBluetoothAddress[] = "11:22:33:44:55:66";

const char fake_local_public_key[] = "fakeLocalPublicKey";

const char current_eid_data[] = "currentEidData";
const int64_t current_eid_start_ms = 1000L;
const int64_t current_eid_end_ms = 2000L;

const char adjacent_eid_data[] = "adjacentEidData";
const int64_t adjacent_eid_start_ms = 2000L;
const int64_t adjacent_eid_end_ms = 3000L;

const char fake_beacon_seed1_data[] = "fakeBeaconSeed1Data";
const int64_t fake_beacon_seed1_start_ms = current_eid_start_ms;
const int64_t fake_beacon_seed1_end_ms = current_eid_end_ms;

const char fake_beacon_seed2_data[] = "fakeBeaconSeed2Data";
const int64_t fake_beacon_seed2_start_ms = adjacent_eid_start_ms;
const int64_t fake_beacon_seed2_end_ms = adjacent_eid_end_ms;

std::unique_ptr<cryptauth::ForegroundEidGenerator::EidData>
CreateFakeBackgroundScanFilter() {
  cryptauth::DataWithTimestamp current(current_eid_data, current_eid_start_ms,
                                       current_eid_end_ms);

  std::unique_ptr<cryptauth::DataWithTimestamp> adjacent =
      base::MakeUnique<cryptauth::DataWithTimestamp>(
          adjacent_eid_data, adjacent_eid_start_ms, adjacent_eid_end_ms);

  return base::MakeUnique<cryptauth::ForegroundEidGenerator::EidData>(
      current, std::move(adjacent));
}

std::vector<cryptauth::BeaconSeed> CreateFakeBeaconSeeds() {
  cryptauth::BeaconSeed seed1;
  seed1.set_data(fake_beacon_seed1_data);
  seed1.set_start_time_millis(fake_beacon_seed1_start_ms);
  seed1.set_start_time_millis(fake_beacon_seed1_end_ms);

  cryptauth::BeaconSeed seed2;
  seed2.set_data(fake_beacon_seed2_data);
  seed2.set_start_time_millis(fake_beacon_seed2_start_ms);
  seed2.set_start_time_millis(fake_beacon_seed2_end_ms);

  std::vector<cryptauth::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}
}  // namespace

class BleScannerImplTest : public testing::Test {
 protected:
  class TestServiceDataProvider : public BleScannerImpl::ServiceDataProvider {
   public:
    TestServiceDataProvider() {}

    ~TestServiceDataProvider() override {}

    // ServiceDataProvider:
    const std::vector<uint8_t>* GetServiceDataForUUID(
        device::BluetoothDevice* bluetooth_device) override {
      return reinterpret_cast<MockBluetoothDeviceWithServiceData*>(
                 bluetooth_device)
          ->service_data();
    }
  };

  BleScannerImplTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(3)),
        test_beacon_seeds_(CreateFakeBeaconSeeds()) {}

  void SetUp() override {
    // Note: This value is only used after the discovery session has been
    // created (i.e., after a StartDiscoverySession() call completes
    // successfully).
    should_discovery_session_be_active_ = true;

    mock_local_device_data_provider_ =
        base::MakeUnique<cryptauth::MockLocalDeviceDataProvider>();
    mock_local_device_data_provider_->SetPublicKey(
        base::MakeUnique<std::string>(fake_local_public_key));
    mock_local_device_data_provider_->SetBeaconSeeds(
        base::MakeUnique<std::vector<cryptauth::BeaconSeed>>(
            test_beacon_seeds_));

    fake_ble_synchronizer_ = base::MakeUnique<FakeBleSynchronizer>();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));

    mock_discovery_session_ = nullptr;

    ble_scanner_ = base::MakeUnique<BleScannerImpl>(
        mock_adapter_, mock_local_device_data_provider_.get(),
        fake_ble_synchronizer_.get());

    mock_eid_generator_ = new cryptauth::MockForegroundEidGenerator();
    mock_eid_generator_->set_background_scan_filter(
        CreateFakeBackgroundScanFilter());
    test_service_data_provider_ = new TestServiceDataProvider();
    test_task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    ble_scanner_->SetTestDoubles(base::WrapUnique(test_service_data_provider_),
                                 base::WrapUnique(mock_eid_generator_),
                                 test_task_runner_);

    test_observer_ = base::MakeUnique<TestBleScannerObserver>();
    ble_scanner_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    EXPECT_EQ(discovery_state_changes_so_far_,
              test_observer_->discovery_session_state_changes());
  }

  void DeviceAdded(MockBluetoothDeviceWithServiceData* device) {
    ble_scanner_->DeviceAdded(mock_adapter_.get(), device);
  }

  void InvokeDiscoveryStartedCallback(bool success, size_t command_index) {
    if (success) {
      mock_discovery_session_ = new device::MockBluetoothDiscoverySession();
      ON_CALL(*mock_discovery_session_, IsActive())
          .WillByDefault(
              Invoke(this, &BleScannerImplTest::MockDiscoveryIsActive));

      fake_ble_synchronizer_->GetStartDiscoveryCallback(command_index)
          .Run(base::WrapUnique(mock_discovery_session_));
      test_task_runner_->RunUntilIdle();
      VerifyDiscoveryStatusChange(true /* discovery_session_active */);
      return;
    }

    fake_ble_synchronizer_->GetStartDiscoveryErrorCallback(command_index).Run();
  }

  bool IsDeviceRegistered(const std::string& device_id) {
    return ble_scanner_->IsDeviceRegistered(device_id);
  }

  void VerifyDiscoveryStatusChange(bool discovery_session_active) {
    discovery_state_changes_so_far_.push_back(discovery_session_active);
    EXPECT_EQ(discovery_state_changes_so_far_,
              test_observer_->discovery_session_state_changes());
  }

  bool MockDiscoveryIsActive() { return should_discovery_session_be_active_; }

  void InvokeStopDiscoveryCallback(bool success, size_t command_index) {
    if (success) {
      fake_ble_synchronizer_->GetStopDiscoveryCallback(command_index).Run();
      test_task_runner_->RunUntilIdle();
      VerifyDiscoveryStatusChange(false /* discovery_session_active */);
      return;
    }

    fake_ble_synchronizer_->GetStopDiscoveryErrorCallback(command_index).Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const std::vector<cryptauth::RemoteDevice> test_devices_;
  const std::vector<cryptauth::BeaconSeed> test_beacon_seeds_;

  std::unique_ptr<cryptauth::MockLocalDeviceDataProvider>
      mock_local_device_data_provider_;
  std::unique_ptr<FakeBleSynchronizer> fake_ble_synchronizer_;

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  device::MockBluetoothDiscoverySession* mock_discovery_session_;

  TestServiceDataProvider* test_service_data_provider_;
  cryptauth::MockForegroundEidGenerator* mock_eid_generator_;
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;

  std::unique_ptr<TestBleScannerObserver> test_observer_;

  bool should_discovery_session_be_active_;

  std::vector<bool> discovery_state_changes_so_far_;

  std::unique_ptr<BleScannerImpl> ble_scanner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleScannerImplTest);
};

TEST_F(BleScannerImplTest, TestNoLocalBeaconSeeds) {
  mock_local_device_data_provider_->SetBeaconSeeds(nullptr);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestNoBackgroundScanFilter) {
  mock_eid_generator_->set_background_scan_filter(nullptr);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscoverySessionFailsToStart) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(false /* success */, 0u /* command_index */);

  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscoveryStartsButNoDevicesFound) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // No devices found.

  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_EQ(0u, test_observer_->device_addresses().size());

  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
}

TEST_F(BleScannerImplTest, TestDiscovery_NoServiceData) {
  std::string empty_service_data = "";

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // Device with no service data connected. Service data is required to identify
  // the advertising device.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress, empty_service_data);
  DeviceAdded(&device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscovery_ServiceDataTooShort) {
  std::string short_service_data = "abc";
  ASSERT_TRUE(short_service_data.size() < kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // Device with short service data connected. Service data of at least 4 bytes
  // is required to identify the advertising device.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress, short_service_data);
  DeviceAdded(&device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscovery_LocalDeviceDataCannotBeFetched) {
  std::string valid_service_data_for_other_device = "abcd";
  ASSERT_TRUE(valid_service_data_for_other_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // Device with valid service data connected, but the local device data
  // cannot be fetched.
  mock_local_device_data_provider_->SetPublicKey(nullptr);
  mock_local_device_data_provider_->SetBeaconSeeds(nullptr);
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_other_device);
  DeviceAdded(&device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscovery_ScanSuccessfulButNoRegisteredDevice) {
  std::string valid_service_data_for_other_device = "abcd";
  ASSERT_TRUE(valid_service_data_for_other_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // Device with valid service data connected, but there was no registered
  // device corresponding to the one that just connected.
  mock_local_device_data_provider_->SetPublicKey(
      base::MakeUnique<std::string>(fake_local_public_key));
  mock_local_device_data_provider_->SetBeaconSeeds(
      base::MakeUnique<std::vector<cryptauth::BeaconSeed>>(test_beacon_seeds_));
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_other_device);
  DeviceAdded(&device);
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(0u, test_observer_->device_addresses().size());
}

TEST_F(BleScannerImplTest, TestDiscovery_Success) {
  std::string valid_service_data_for_registered_device = "abcde";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  // Registered device connects.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_registered_device);
  mock_eid_generator_->set_identified_device(&test_devices_[0]);
  DeviceAdded(&device);
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(1u, test_observer_->device_addresses().size());
  EXPECT_EQ(device.GetAddress(), test_observer_->device_addresses()[0]);
  EXPECT_EQ(1u, test_observer_->remote_devices().size());
  EXPECT_EQ(test_devices_[0], test_observer_->remote_devices()[0]);

  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(1u, test_observer_->device_addresses().size());
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
}

TEST_F(BleScannerImplTest, TestDiscovery_MultipleObservers) {
  TestBleScannerObserver extra_observer;
  ble_scanner_->AddObserver(&extra_observer);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);

  MockBluetoothDeviceWithServiceData mock_bluetooth_device(
      mock_adapter_.get(), kDefaultBluetoothAddress, "fakeServiceData");
  mock_eid_generator_->set_identified_device(&test_devices_[0]);
  DeviceAdded(&mock_bluetooth_device);

  EXPECT_EQ(1u, test_observer_->device_addresses().size());
  EXPECT_EQ(mock_bluetooth_device.GetAddress(),
            test_observer_->device_addresses()[0]);
  EXPECT_EQ(1u, test_observer_->remote_devices().size());
  EXPECT_EQ(test_devices_[0], test_observer_->remote_devices()[0]);

  EXPECT_EQ(1u, extra_observer.device_addresses().size());
  EXPECT_EQ(mock_bluetooth_device.GetAddress(),
            extra_observer.device_addresses()[0]);
  EXPECT_EQ(1u, extra_observer.remote_devices().size());
  EXPECT_EQ(test_devices_[0], extra_observer.remote_devices()[0]);

  // Now, unregister both observers.
  ble_scanner_->RemoveObserver(test_observer_.get());
  ble_scanner_->RemoveObserver(&extra_observer);

  // Now, simulate another scan being received. The observers should not be
  // notified since they are unregistered, so they should still have a call
  // count of 1.
  DeviceAdded(&mock_bluetooth_device);
  EXPECT_EQ(1u, test_observer_->device_addresses().size());
  EXPECT_EQ(1u, extra_observer.device_addresses().size());

  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));

  // Note: Cannot use InvokeStopDiscoveryCallback() since that function
  // internally verifies observer callbacks, but the observers have been
  // unregistered in this case.
  fake_ble_synchronizer_->GetStopDiscoveryCallback(1u /* command_index */)
      .Run();
}

TEST_F(BleScannerImplTest, TestRegistrationLimit) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[1]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[1].GetDeviceId()));

  // Attempt to register another device. Registration should fail since the
  // maximum number of devices have already been registered.
  ASSERT_EQ(2u, kMaxConcurrentAdvertisements);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Unregistering a device which is not registered should also return false.
  EXPECT_FALSE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Unregister device 0.
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  // Now, device 2 can be registered.
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[2]));
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Now, unregister the devices.
  EXPECT_TRUE(IsDeviceRegistered(test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[1]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[1].GetDeviceId()));

  EXPECT_TRUE(IsDeviceRegistered(test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(IsDeviceRegistered(test_devices_[2].GetDeviceId()));
}

TEST_F(BleScannerImplTest, TestStartAndStopCallbacks_Success) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());

  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());
  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[1]));

  // Registering device 1 should not have triggered a new discovery session from
  // being created since one already existed.
  EXPECT_EQ(1u, fake_ble_synchronizer_->GetNumCommands());
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));

  // Unregistering device 0 should not have triggered a stopped session since
  // a device is still registered.
  EXPECT_EQ(1u, fake_ble_synchronizer_->GetNumCommands());
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Device 1 is the only device remaining, so unregistering it should trigger
  // the session to stop.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[1]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());
}

TEST_F(BleScannerImplTest, TestStartAndStopCallbacks_Errors) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Fail to start discovery session.
  InvokeDiscoveryStartedCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Since the previous try failed, a new one should have been attempted. Let
  // that one fail as well.
  InvokeDiscoveryStartedCallback(false /* success */, 1u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Now, let it succeed.
  InvokeDiscoveryStartedCallback(true /* success */, 2u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Now, unregister it, but fail to stop the discovery session.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());
  InvokeStopDiscoveryCallback(false /* success */, 3u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Since the previous try failed, a new stop should have been attempted. Let
  // that one fail as well.
  InvokeStopDiscoveryCallback(false /* success */, 4u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Now, let it succeed.
  InvokeStopDiscoveryCallback(true /* success */, 5u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());
}

TEST_F(BleScannerImplTest, TestStartAndStopCallbacks_UnregisterBeforeStarted) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Before invoking the discovery callback, unregister the device.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());

  // Complete the start discovery successfully.
  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Because the session should not be active (i.e., there are no registered
  // devices), a stop should be triggered.
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());
}

TEST_F(BleScannerImplTest,
       TestStartAndStopCallbacks_UnregisterBeforeStartFails) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Before invoking the discovery callback, unregister the device.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());

  // Fail to start discovery session.
  InvokeDiscoveryStartedCallback(false /* success */, 0u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Because the session should not be active (i.e., there are no registered
  // devices), a new attempt should not have occurred.
  EXPECT_EQ(1u, fake_ble_synchronizer_->GetNumCommands());
}

TEST_F(BleScannerImplTest, TestStartAndStopCallbacks_RegisterBeforeStopFails) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Start discovery session.
  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Unregister device to attempt a stop.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Before the stop completes, register the device again.
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Fail to stop.
  InvokeStopDiscoveryCallback(false /* success */, 1u /* command_index */);
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Since there is a device registered again, there should not be another
  // attempt to stop.
  EXPECT_EQ(2u, fake_ble_synchronizer_->GetNumCommands());
}

// Regression test for crbug.com/768521.
TEST_F(BleScannerImplTest,
       TestStopCallback_DiscoverySessionInactiveButNotStopped) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());

  // Start discovery session.
  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);
  test_task_runner_->RunUntilIdle();
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // Unregister device to attempt a stop.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(ble_scanner_->ShouldDiscoverySessionBeActive());
  EXPECT_TRUE(ble_scanner_->IsDiscoverySessionActive());

  // For this test, simulate the discovery session transitioning to
  // IsActive() == false without Stop() ever succeeding.
  should_discovery_session_be_active_ = false;

  // Fail to stop. Even though stopping failed, IsActive() will still return
  // false. In this case, the discovery session should no longer be active.
  InvokeStopDiscoveryCallback(false /* success */, 1u /* command_index */);
  EXPECT_FALSE(ble_scanner_->IsDiscoverySessionActive());
  test_task_runner_->RunUntilIdle();
  VerifyDiscoveryStatusChange(false /* discovery_session_active */);

  // Since the discovery session was not active, there should not have been an
  // additional call to Stop().
  EXPECT_EQ(2u, fake_ble_synchronizer_->GetNumCommands());
}

// Regression test for crbug.com/776241. This bug could cause a crash if, when
// BleScannerImpl notifies observers that all the discovery session has stopped,
// an observer deletes BleScannerImpl. The fix for this issue is simply
// notifying observers in a new task so that no further action will be taken if
// the object is deleted. Without the fix for crbug.com/776241, this test would
// crash.
TEST_F(BleScannerImplTest, ObserverDeletesObjectWhenNotified) {
  DeletingObserver deleting_observer(ble_scanner_);

  ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]);
  InvokeDiscoveryStartedCallback(true /* success */, 0u /* command_index */);
  test_task_runner_->RunUntilIdle();
  ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]);
  InvokeStopDiscoveryCallback(true /* success */, 1u /* command_index */);
  test_task_runner_->RunUntilIdle();
}

}  // namespace tether

}  // namespace chromeos
