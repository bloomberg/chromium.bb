// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_scanner.h"

#include "base/logging.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/mock_local_device_data_provider.h"
#include "components/cryptauth/mock_eid_generator.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::SaveArg;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {

class MockBleScannerObserver : public BleScanner::Observer {
 public:
  MockBleScannerObserver() {}

  // BleScanner::Observer:
  void OnReceivedAdvertisementFromDevice(
      const std::string& device_address,
      cryptauth::RemoteDevice remote_device) override {
    device_addresses_.push_back(device_address);
    remote_devices_.push_back(remote_device);
  }

  int GetNumCalls() { return static_cast<int>(device_addresses_.size()); }

  std::vector<std::string>& device_addresses() { return device_addresses_; }

  std::vector<cryptauth::RemoteDevice>& remote_devices() {
    return remote_devices_;
  }

 private:
  std::vector<std::string> device_addresses_;
  std::vector<cryptauth::RemoteDevice> remote_devices_;
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

const int kExpectedDiscoveryRSSI = -90;
const size_t kMinNumBytesInServiceData = 4;

const std::string kDefaultBluetoothAddress = "11:22:33:44:55:66";

const std::string fake_local_public_key = "fakeLocalPublicKey";

const std::string current_eid_data = "currentEidData";
const int64_t current_eid_start_ms = 1000L;
const int64_t current_eid_end_ms = 2000L;

const std::string adjacent_eid_data = "adjacentEidData";
const int64_t adjacent_eid_start_ms = 2000L;
const int64_t adjacent_eid_end_ms = 3000L;

const std::string fake_beacon_seed1_data = "fakeBeaconSeed1Data";
const int64_t fake_beacon_seed1_start_ms = current_eid_start_ms;
const int64_t fake_beacon_seed1_end_ms = current_eid_end_ms;

const std::string fake_beacon_seed2_data = "fakeBeaconSeed2Data";
const int64_t fake_beacon_seed2_start_ms = adjacent_eid_start_ms;
const int64_t fake_beacon_seed2_end_ms = adjacent_eid_end_ms;

std::unique_ptr<cryptauth::EidGenerator::EidData>
CreateFakeBackgroundScanFilter() {
  cryptauth::EidGenerator::DataWithTimestamp current(
      current_eid_data, current_eid_start_ms, current_eid_end_ms);

  std::unique_ptr<cryptauth::EidGenerator::DataWithTimestamp> adjacent =
      base::MakeUnique<cryptauth::EidGenerator::DataWithTimestamp>(
          adjacent_eid_data, adjacent_eid_start_ms, adjacent_eid_end_ms);

  return base::MakeUnique<cryptauth::EidGenerator::EidData>(
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

class BleScannerTest : public testing::Test {
 protected:
  class TestDelegate : public BleScanner::Delegate {
   public:
    TestDelegate()
        : is_bluetooth_adapter_available_(true),
          last_get_adapter_callback_(nullptr) {}

    ~TestDelegate() override {}

    bool IsBluetoothAdapterAvailable() const override {
      return is_bluetooth_adapter_available_;
    }

    void set_is_bluetooth_adapter_available(
        bool is_bluetooth_adapter_available) {
      is_bluetooth_adapter_available_ = is_bluetooth_adapter_available;
    }

    void GetAdapter(const device::BluetoothAdapterFactory::AdapterCallback&
                        callback) override {
      last_get_adapter_callback_ = callback;
    }

    const device::BluetoothAdapterFactory::AdapterCallback
    last_get_adapter_callback() {
      return last_get_adapter_callback_;
    }

    const std::vector<uint8_t>* GetServiceDataForUUID(
        const device::BluetoothUUID& service_uuid,
        device::BluetoothDevice* bluetooth_device) override {
      if (device::BluetoothUUID(kAdvertisingServiceUuid) == service_uuid) {
        return reinterpret_cast<MockBluetoothDeviceWithServiceData*>(
                   bluetooth_device)
            ->service_data();
      }

      return nullptr;
    }

   private:
    bool is_bluetooth_adapter_available_;
    device::BluetoothAdapterFactory::AdapterCallback last_get_adapter_callback_;
  };

  BleScannerTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(3)),
        test_beacon_seeds_(CreateFakeBeaconSeeds()) {}

  void SetUp() override {
    test_delegate_ = new TestDelegate();
    EXPECT_TRUE(test_delegate_->IsBluetoothAdapterAvailable());
    EXPECT_FALSE(test_delegate_->last_get_adapter_callback());

    mock_eid_generator_ = base::MakeUnique<cryptauth::MockEidGenerator>();
    mock_eid_generator_->set_background_scan_filter(
        CreateFakeBackgroundScanFilter());

    mock_local_device_data_provider_ =
        base::MakeUnique<MockLocalDeviceDataProvider>();
    mock_local_device_data_provider_->SetPublicKey(
        base::MakeUnique<std::string>(fake_local_public_key));
    mock_local_device_data_provider_->SetBeaconSeeds(
        base::MakeUnique<std::vector<cryptauth::BeaconSeed>>(
            test_beacon_seeds_));

    mock_adapter_ =
        make_scoped_refptr(new NiceMock<device::MockBluetoothAdapter>());
    stored_discovery_filter_.reset();
    stored_discovery_callback_.Reset();
    stored_discovery_errback_.Reset();
    ON_CALL(*mock_adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
        .WillByDefault(Invoke(
            this, &BleScannerTest::SaveStartDiscoverySessionWithFilterArgs));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));

    mock_discovery_session_ = nullptr;

    ble_scanner_ = base::WrapUnique(new BleScanner(
        base::WrapUnique(test_delegate_), mock_eid_generator_.get(),
        mock_local_device_data_provider_.get()));

    mock_observer_ = base::MakeUnique<MockBleScannerObserver>();
    ble_scanner_->AddObserver(mock_observer_.get());
  }

  void SaveStartDiscoverySessionWithFilterArgs(
      const device::BluetoothDiscoveryFilter* discovery_filter,
      const device::BluetoothAdapter::DiscoverySessionCallback& callback,
      const device::BluetoothAdapter::ErrorCallback& errback) {
    stored_discovery_filter_ =
        base::MakeUnique<device::BluetoothDiscoveryFilter>(
            device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE);
    stored_discovery_filter_->CopyFrom(*discovery_filter);
    stored_discovery_callback_ = callback;
    stored_discovery_errback_ = errback;
  }

  void InvokeAdapterCallback() {
    const device::BluetoothAdapterFactory::AdapterCallback
        last_get_adapter_callback = test_delegate_->last_get_adapter_callback();
    ASSERT_TRUE(last_get_adapter_callback);

    // Because the adapter has just been initialized, the discovery session
    // should not have been started yet.
    EXPECT_FALSE(stored_discovery_filter_);
    EXPECT_TRUE(stored_discovery_callback_.is_null());
    EXPECT_TRUE(stored_discovery_errback_.is_null());

    EXPECT_CALL(*mock_adapter_, AddObserver(ble_scanner_.get()));
    last_get_adapter_callback.Run(mock_adapter_);

    // Once the adapter callback is returned, a discovery session should be
    // started via that adapter.
    AssertDiscoverySessionRequested();
  }

  void AssertDiscoverySessionRequested() {
    // First, ensure that the correct discovery filter was passed.
    EXPECT_TRUE(stored_discovery_filter_);
    EXPECT_EQ(device::BluetoothTransport::BLUETOOTH_TRANSPORT_LE,
              stored_discovery_filter_->GetTransport());
    int16_t observed_rssi;
    ASSERT_TRUE(stored_discovery_filter_->GetRSSI(&observed_rssi));
    EXPECT_EQ(kExpectedDiscoveryRSSI, observed_rssi);

    // Now, ensure that both a callback and errback were passed.
    EXPECT_FALSE(stored_discovery_callback_.is_null());
    EXPECT_FALSE(stored_discovery_errback_.is_null());
  }

  void InvokeDiscoveryStartedCallback() {
    EXPECT_FALSE(stored_discovery_callback_.is_null());

    mock_discovery_session_ = new device::MockBluetoothDiscoverySession();
    stored_discovery_callback_.Run(base::WrapUnique(mock_discovery_session_));
  }

  std::vector<cryptauth::RemoteDevice> test_devices_;
  std::vector<cryptauth::BeaconSeed> test_beacon_seeds_;

  std::unique_ptr<MockBleScannerObserver> mock_observer_;

  TestDelegate* test_delegate_;
  std::unique_ptr<cryptauth::MockEidGenerator> mock_eid_generator_;
  std::unique_ptr<MockLocalDeviceDataProvider> mock_local_device_data_provider_;

  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  device::MockBluetoothDiscoverySession* mock_discovery_session_;

  std::unique_ptr<device::BluetoothDiscoveryFilter> stored_discovery_filter_;
  device::BluetoothAdapter::DiscoverySessionCallback stored_discovery_callback_;
  device::BluetoothAdapter::ErrorCallback stored_discovery_errback_;

  std::unique_ptr<BleScanner> ble_scanner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleScannerTest);
};

TEST_F(BleScannerTest, TestNoBluetoothAdapter) {
  test_delegate_->set_is_bluetooth_adapter_available(false);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(test_delegate_->last_get_adapter_callback());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestNoLocalBeaconSeeds) {
  mock_local_device_data_provider_->SetBeaconSeeds(nullptr);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(test_delegate_->last_get_adapter_callback());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestNoBackgroundScanFilter) {
  mock_eid_generator_->set_background_scan_filter(nullptr);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(test_delegate_->last_get_adapter_callback());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestAdapterDoesNotInitialize) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(test_delegate_->last_get_adapter_callback());

  // Do not call the last GetAdapter() callback. The device should still be able
  // to be unregistered.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestAdapterDoesNotInitialize_MultipleDevicesRegistered) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[1]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(test_delegate_->last_get_adapter_callback());

  // Do not call the last GetAdapter() callback. The devices should still be
  // able to be unregistered.
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[1]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[1].GetDeviceId()));
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscoverySessionFailsToStart) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  stored_discovery_errback_.Run();

  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscoveryStartsButNoDevicesFound) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // No devices found.

  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_NoServiceData) {
  std::string empty_service_data = "";

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // Device with no service data connected. Service data is required to identify
  // the advertising device.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress, empty_service_data);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_ServiceDataTooShort) {
  std::string short_service_data = "abc";
  ASSERT_TRUE(short_service_data.size() < kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // Device with short service data connected. Service data of at least 4 bytes
  // is required to identify the advertising device.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress, short_service_data);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_LocalDeviceDataCannotBeFetched) {
  std::string valid_service_data_for_other_device = "abcd";
  ASSERT_TRUE(valid_service_data_for_other_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // Device with valid service data connected, but the local device data
  // cannot be fetched.
  mock_local_device_data_provider_->SetPublicKey(nullptr);
  mock_local_device_data_provider_->SetBeaconSeeds(nullptr);
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_other_device);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &device);
  EXPECT_FALSE(mock_eid_generator_->num_identify_calls());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_ScanSuccessfulButNoRegisteredDevice) {
  std::string valid_service_data_for_other_device = "abcd";
  ASSERT_TRUE(valid_service_data_for_other_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // Device with valid service data connected, but there was no registered
  // device corresponding to the one that just connected.
  mock_local_device_data_provider_->SetPublicKey(
      base::MakeUnique<std::string>(fake_local_public_key));
  mock_local_device_data_provider_->SetBeaconSeeds(
      base::MakeUnique<std::vector<cryptauth::BeaconSeed>>(test_beacon_seeds_));
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_other_device);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &device);
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_FALSE(mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_Success) {
  std::string valid_service_data_for_registered_device = "abcde";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kMinNumBytesInServiceData);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  // Registered device connects.
  MockBluetoothDeviceWithServiceData device(
      mock_adapter_.get(), kDefaultBluetoothAddress,
      valid_service_data_for_registered_device);
  mock_eid_generator_->set_identified_device(&test_devices_[0]);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &device);
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(1, mock_observer_->GetNumCalls());
  EXPECT_EQ(1, static_cast<int>(mock_observer_->device_addresses().size()));
  EXPECT_EQ(device.GetAddress(), mock_observer_->device_addresses()[0]);
  EXPECT_EQ(1, static_cast<int>(mock_observer_->remote_devices().size()));
  EXPECT_EQ(test_devices_[0], mock_observer_->remote_devices()[0]);

  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_EQ(1, mock_eid_generator_->num_identify_calls());
  EXPECT_EQ(1, mock_observer_->GetNumCalls());
}

TEST_F(BleScannerTest, TestDiscovery_MultipleObservers) {
  MockBleScannerObserver extra_observer;
  ble_scanner_->AddObserver(&extra_observer);

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  InvokeAdapterCallback();
  InvokeDiscoveryStartedCallback();

  MockBluetoothDeviceWithServiceData mock_bluetooth_device(
      mock_adapter_.get(), kDefaultBluetoothAddress, "fakeServiceData");
  mock_eid_generator_->set_identified_device(&test_devices_[0]);
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &mock_bluetooth_device);

  EXPECT_EQ(1, mock_observer_->GetNumCalls());
  EXPECT_EQ(1, static_cast<int>(mock_observer_->device_addresses().size()));
  EXPECT_EQ(mock_bluetooth_device.GetAddress(),
            mock_observer_->device_addresses()[0]);
  EXPECT_EQ(1, static_cast<int>(mock_observer_->remote_devices().size()));
  EXPECT_EQ(test_devices_[0], mock_observer_->remote_devices()[0]);

  EXPECT_EQ(1, extra_observer.GetNumCalls());
  EXPECT_EQ(1, static_cast<int>(extra_observer.device_addresses().size()));
  EXPECT_EQ(mock_bluetooth_device.GetAddress(),
            extra_observer.device_addresses()[0]);
  EXPECT_EQ(1, static_cast<int>(extra_observer.remote_devices().size()));
  EXPECT_EQ(test_devices_[0], extra_observer.remote_devices()[0]);

  // Now, unregister both observers.
  ble_scanner_->RemoveObserver(mock_observer_.get());
  ble_scanner_->RemoveObserver(&extra_observer);

  // Now, simulate another scan being received. The observers should not be
  // notified since they are unregistered, so they should still have a call
  // count of 1.
  ble_scanner_->DeviceAdded(mock_adapter_.get(), &mock_bluetooth_device);
  EXPECT_EQ(1, mock_observer_->GetNumCalls());
  EXPECT_EQ(1, extra_observer.GetNumCalls());

  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
}

TEST_F(BleScannerTest, TestRegistrationLimit) {
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[1]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[1].GetDeviceId()));

  // Attempt to register another device. Registration should fail since the
  // maximum number of devices have already been registered.
  ASSERT_EQ(2, kMaxConcurrentAdvertisements);
  EXPECT_FALSE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Unregistering a device which is not registered should also return false.
  EXPECT_FALSE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Unregister device 0.
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));

  // Now, device 2 can be registered.
  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[2]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[2].GetDeviceId()));

  // Now, unregister the devices.
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[1].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[1]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[1].GetDeviceId()));

  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[2].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[2]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[2].GetDeviceId()));
}

TEST_F(BleScannerTest, TestAdapterPoweredChanged) {
  // This test starts with the adapter powered on, then turns power off before
  // discovery starts, then turns power back on and allows discovery to
  // complete, then turns power back off again, and finally turns it back on to
  // complete another discovery session.
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_TRUE(ble_scanner_->RegisterScanFilterForDevice(test_devices_[0]));
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  InvokeAdapterCallback();

  // The discovery session should have been requested but not yet initialized.
  EXPECT_TRUE(stored_discovery_filter_.get());
  EXPECT_FALSE(mock_discovery_session_);

  // Turn the adapter off before the discovery session starts.
  ble_scanner_->AdapterPoweredChanged(mock_adapter_.get(), false);

  // Turn the adapter back on, and finish initializing discovery this time.
  ble_scanner_->AdapterPoweredChanged(mock_adapter_.get(), true);
  InvokeDiscoveryStartedCallback();

  // The session should have been started.
  device::MockBluetoothDiscoverySession* session1 = mock_discovery_session_;
  EXPECT_NE(nullptr, session1);

  // Now turn the adapter off again.
  ble_scanner_->AdapterPoweredChanged(mock_adapter_.get(), false);

  // Turn the adapter back on.
  ble_scanner_->AdapterPoweredChanged(mock_adapter_.get(), true);
  InvokeDiscoveryStartedCallback();

  // A new session should have started, so the session objects should not be the
  // same.
  device::MockBluetoothDiscoverySession* session2 = mock_discovery_session_;
  EXPECT_NE(nullptr, session2);
  EXPECT_NE(session1, session2);

  // Unregister device.
  EXPECT_TRUE(ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
  EXPECT_TRUE(ble_scanner_->UnregisterScanFilterForDevice(test_devices_[0]));
  EXPECT_FALSE(
      ble_scanner_->IsDeviceRegistered(test_devices_[0].GetDeviceId()));
}

}  // namespace tether

}  // namespace chromeos
