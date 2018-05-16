// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_cable_discovery.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_advertisement.h"
#include "device/fido/fido_ble_device.h"
#include "device/fido/fido_ble_uuids.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;

namespace device {

namespace {

// Constants required for discovering and constructing a Cable device that
// are given by the relying party via an extension.
constexpr FidoCableDiscovery::EidArray kClientEid = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00}};

constexpr FidoCableDiscovery::EidArray kAuthenticatorEid = {
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
     0x01, 0x01, 0x01, 0x01}};

constexpr FidoCableDiscovery::EidArray kInvalidAuthenticatorEid = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00}};

constexpr FidoCableDiscovery::SessionKeyArray kTestSessionKey = {
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";

constexpr char kTestBleDeviceName[] = "test_cable_device";

std::unique_ptr<MockBluetoothDevice> CreateTestBluetoothDevice() {
  return std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
      nullptr /* adapter */, 0 /* bluetooth_class */, kTestBleDeviceName,
      kTestBleDeviceAddress, true /* paired */, true /* connected */);
}

// Mock BLE adapter that abstracts out authenticator logic with the following
// logic:
//  - Responds to BluetoothAdapter::RegisterAdvertisement() by always invoking
//    success callback.
//  - Responds to BluetoothAdapter::StartDiscoverySessionWithFilter() by
//    invoking BluetoothAdapter::Observer::DeviceAdded() on a test bluetooth
//    device that includes service data containing authenticator EID.
class CableMockAdapter : public MockBluetoothAdapter {
 public:
  MOCK_METHOD3(RegisterAdvertisement,
               void(std::unique_ptr<BluetoothAdvertisement::Data>,
                    const CreateAdvertisementCallback&,
                    const AdvertisementErrorCallback&));

  void AddNewTestBluetoothDevice(
      base::span<const uint8_t, FidoCableDiscovery::kEphemeralIdSize>
          authenticator_eid) {
    auto mock_device = CreateTestBluetoothDevice();

    std::vector<uint8_t> service_data(8);
    fido_parsing_utils::Append(&service_data, authenticator_eid);
    BluetoothDevice::ServiceDataMap service_data_map;
    service_data_map.emplace(kCableAdvertisementUUID, std::move(service_data));

    mock_device->UpdateAdvertisementData(
        1 /* rssi */, BluetoothDevice::UUIDList(), std::move(service_data_map),
        BluetoothDevice::ManufacturerDataMap(), nullptr /* tx_power*/);

    auto* mock_device_ptr = mock_device.get();
    AddMockDevice(std::move(mock_device));

    for (auto& observer : GetObservers())
      observer.DeviceAdded(this, mock_device_ptr);
  }

  void ExpectRegisterAdvertisementWithSuccessResponse(size_t num_called) {
    EXPECT_CALL(*this, RegisterAdvertisement(_, _, _))
        .Times(num_called)
        .WillRepeatedly(::testing::WithArg<1>([](const auto& callback) {
          callback.Run(base::MakeRefCounted<MockBluetoothAdvertisement>());
        }));
  }

  void ExpectSuccessCallbackToSetPowered() {
    EXPECT_CALL(*this, SetPowered(true, _, _))
        .WillOnce(::testing::WithArg<1>(
            [](const auto& callback) { callback.Run(); }));
  }

 protected:
  ~CableMockAdapter() override = default;
};

}  // namespace

class FidoCableDiscoveryTest : public ::testing::Test {
 public:
  std::unique_ptr<FidoCableDiscovery> CreateDiscovery() {
    std::vector<FidoCableDiscovery::CableDiscoveryData> discovery_data;
    discovery_data.emplace_back(kClientEid, kAuthenticatorEid, kTestSessionKey);
    return std::make_unique<FidoCableDiscovery>(std::move(discovery_data));
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

// Tests regular successful discovery flow for Cable device.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFindsNewDevice) {
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer, DeviceAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToSetPowered();

  EXPECT_CALL(*mock_adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(::testing::WithArg<1>([&mock_adapter](const auto& callback) {
        mock_adapter->AddNewTestBluetoothDevice(kAuthenticatorEid);
        callback.Run(nullptr);
      }));
  mock_adapter->ExpectRegisterAdvertisementWithSuccessResponse(1);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  scoped_task_environment_.RunUntilIdle();
}

// Tests a scenario where upon broadcasting advertisement and scanning, client
// discovers a device with an incorrect authenticator EID. Observer::AddDevice()
// must not be called.
TEST_F(FidoCableDiscoveryTest, TestDiscoveryFindsIncorrectDevice) {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  auto cable_discovery = CreateDiscovery();
  NiceMock<MockFidoDiscoveryObserver> mock_observer;

  EXPECT_CALL(mock_observer, DeviceAdded(_, _)).Times(0);
  cable_discovery->set_observer(&mock_observer);
  mock_adapter->ExpectSuccessCallbackToSetPowered();

  EXPECT_CALL(*mock_adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(::testing::WithArg<1>([&mock_adapter](const auto& callback) {
        mock_adapter->AddNewTestBluetoothDevice(kInvalidAuthenticatorEid);
        callback.Run(nullptr);
      }));
  mock_adapter->ExpectRegisterAdvertisementWithSuccessResponse(1);

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  scoped_task_environment_.RunUntilIdle();
}

// Tests Cable discovery flow when multiple(2) sets of client/authenticator EIDs
// are passed on from the relying party. We should expect 2 invocations of
// BluetoothAdapter::RegisterAdvertisement().
TEST_F(FidoCableDiscoveryTest, TestDiscoveryWithMultipleEids) {
  constexpr FidoCableDiscovery::EidArray kSecondaryClientEid = {
      {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
       0xff, 0xff, 0xff, 0xff}};

  constexpr FidoCableDiscovery::EidArray kSecondaryAuthenticatorEid = {
      {0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee,
       0xee, 0xee, 0xee, 0xee}};

  constexpr FidoCableDiscovery::SessionKeyArray kSecondarySessionKey = {
      {0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
       0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
       0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd}};

  std::vector<FidoCableDiscovery::CableDiscoveryData> discovery_data;
  discovery_data.emplace_back(kClientEid, kAuthenticatorEid, kTestSessionKey);
  discovery_data.emplace_back(kSecondaryClientEid, kSecondaryAuthenticatorEid,
                              kSecondarySessionKey);
  auto cable_discovery =
      std::make_unique<FidoCableDiscovery>(std::move(discovery_data));

  NiceMock<MockFidoDiscoveryObserver> mock_observer;
  EXPECT_CALL(mock_observer, DeviceAdded(_, _));
  cable_discovery->set_observer(&mock_observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<CableMockAdapter>>();
  mock_adapter->ExpectSuccessCallbackToSetPowered();
  mock_adapter->ExpectRegisterAdvertisementWithSuccessResponse(2);
  EXPECT_CALL(*mock_adapter, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(::testing::WithArg<1>([&mock_adapter](const auto& callback) {
        mock_adapter->AddNewTestBluetoothDevice(kAuthenticatorEid);
        callback.Run(nullptr);
      }));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  cable_discovery->Start();
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace device
