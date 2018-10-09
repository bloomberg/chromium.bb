// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_discovery.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/ble/fido_ble_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/mock_fido_discovery_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#endif

namespace device {

namespace {

using ::testing::_;
using TestMockDevice = ::testing::NiceMock<MockBluetoothDevice>;

constexpr char kDeviceName[] = "device_name";
constexpr char kDeviceAddress[] = "device_address";
constexpr char kDeviceChangedAddress[] = "device_changed_address";
constexpr char kAuthenticatorId[] = "ble:device_address";
constexpr char kAuthenticatorChangedId[] = "ble:device_changed_address";

ACTION_P(ReturnFromAsyncCall, closure) {
  closure.Run();
}

MATCHER_P(IdMatches, id, "") {
  return arg->GetId() == std::string("ble:") + id;
}

}  // namespace

TEST_F(BluetoothTest, FidoBleDiscoveryNotifyObserverWhenAdapterNotPresent) {
  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(false));
  EXPECT_CALL(*mock_adapter, SetPowered).Times(0);
  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, false));
  discovery.Start();
}

TEST_F(BluetoothTest, FidoBleDiscoveryResumeScanningAfterPoweredOn) {
  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(true));
  EXPECT_CALL(*mock_adapter, IsPowered()).WillOnce(::testing::Return(false));

  // After BluetoothAdapter is powered on, we expect that discovery session
  // starts again.
  EXPECT_CALL(*mock_adapter, StartDiscoverySessionWithFilterRaw);
  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  discovery.Start();
  mock_adapter->NotifyAdapterPoweredChanged(true);
}

TEST_F(BluetoothTest, FidoBleDiscoveryNoAdapter) {
  // We purposefully construct a temporary and provide no fake adapter,
  // simulating cases where the discovery is destroyed before obtaining a handle
  // to an adapter. This should be handled gracefully and not result in a crash.
  FidoBleDiscovery discovery;

  // We don't expect any calls to the notification methods.
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);
  EXPECT_CALL(observer, DiscoveryStarted(&discovery, _)).Times(0);
  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _)).Times(0);
  EXPECT_CALL(observer, AuthenticatorRemoved(&discovery, _)).Times(0);
}

TEST_F(BluetoothTest, FidoBleDiscoveryFindsKnownDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  SimulateLowEnergyDevice(4);  // This device should be ignored.
  SimulateLowEnergyDevice(7);

  FidoBleDiscovery discovery;

  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(
        observer,
        AuthenticatorAdded(&discovery,
                           IdMatches(BluetoothTestBase::kTestDeviceAddress1)));
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }
}

TEST_F(BluetoothTest, FidoBleDiscoveryFindsNewDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(
        observer,
        AuthenticatorAdded(&discovery,
                           IdMatches(BluetoothTestBase::kTestDeviceAddress1)))
        .WillOnce(ReturnFromAsyncCall(quit));

    SimulateLowEnergyDevice(4);  // This device should be ignored.
    SimulateLowEnergyDevice(7);

    run_loop.Run();
  }
}

// Simulate the scenario where the BLE device is already known at start-up time,
// but no service advertisements have been received from the device yet, so we
// do not know if it is a CTAP2/U2F device or not. As soon as it is discovered
// that the device supports the FIDO service, the observer should be notified of
// a new FidoBleDevice.
TEST_F(BluetoothTest, FidoBleDiscoveryFindsUpdatedDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  SimulateLowEnergyDevice(3);

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();

    EXPECT_THAT(discovery.GetAuthenticatorsForTesting(), ::testing::IsEmpty());
  }

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(
        observer,
        AuthenticatorAdded(&discovery,
                           IdMatches(BluetoothTestBase::kTestDeviceAddress1)))
        .WillOnce(ReturnFromAsyncCall(quit));

    // This will update properties for device 3.
    SimulateLowEnergyDevice(7);

    run_loop.Run();

    const auto authenticators = discovery.GetAuthenticatorsForTesting();
    ASSERT_THAT(authenticators, ::testing::SizeIs(1u));
    EXPECT_EQ(FidoBleDevice::GetId(BluetoothTestBase::kTestDeviceAddress1),
              authenticators[0]->GetId());
  }
}

TEST_F(BluetoothTest, FidoBleDiscoveryRejectsCableDevice) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  {
    base::RunLoop run_loop;
    auto quit = run_loop.QuitClosure();
    EXPECT_CALL(observer, DiscoveryStarted(&discovery, true))
        .WillOnce(ReturnFromAsyncCall(quit));

    discovery.Start();
    run_loop.Run();
  }

  EXPECT_CALL(observer, AuthenticatorAdded(&discovery, _)).Times(0);

  // Simulates a discovery of two Cable devices one of which is an Android Cable
  // authenticator and other is IOS Cable authenticator.
  SimulateLowEnergyDevice(8);
  SimulateLowEnergyDevice(9);

  // Simulates a device change update received from the BluetoothAdapter. As the
  // updated device has an address that we know is an Cable device, this should
  // not trigger AuthenticatorAdded().
  SimulateLowEnergyDevice(7);
}

TEST_F(BluetoothTest, DiscoveryDoesNotAddDuplicateDeviceOnAddressChanged) {
  using TestMockDevice = ::testing::NiceMock<MockBluetoothDevice>;

  MockFidoDiscoveryObserver observer;
  FidoBleDiscovery discovery;
  discovery.set_observer(&observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(true));

  auto mock_device = std::make_unique<TestMockDevice>(
      mock_adapter.get(), 0 /* bluetooth_class */, kDeviceName, kDeviceAddress,
      false /* paired */, false /* connected */);

  EXPECT_CALL(*mock_device.get(), GetUUIDs)
      .WillRepeatedly(::testing::Return(
          std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)}));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter.get());
  EXPECT_CALL(observer, AuthenticatorIdChanged(&discovery, kAuthenticatorId,
                                               kAuthenticatorChangedId));
  discovery.Start();

  EXPECT_CALL(*mock_device.get(), GetAddress)
      .WillRepeatedly(::testing::Return(kDeviceAddress));
  mock_adapter->NotifyDeviceChanged(mock_device.get());
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_device.get()));

  EXPECT_CALL(*mock_device.get(), GetAddress)
      .WillRepeatedly(::testing::Return(kDeviceChangedAddress));
  for (auto& observer : mock_adapter->GetObservers()) {
    observer.DeviceAddressChanged(mock_adapter.get(), mock_device.get(),
                                  kDeviceAddress);
  }

  mock_adapter->NotifyDeviceChanged(mock_device.get());

  EXPECT_EQ(1u, discovery.GetAuthenticatorsForTesting().size());
  EXPECT_TRUE(discovery.GetAuthenticatorForTesting(kAuthenticatorChangedId));
}

TEST_F(BluetoothTest, DiscoveryNotifiesObserverWhenDeviceInPairingMode) {
  FidoBleDiscovery discovery;
  MockFidoDiscoveryObserver observer;
  discovery.set_observer(&observer);

  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent()).WillOnce(::testing::Return(true));

  auto mock_device = std::make_unique<TestMockDevice>(
      mock_adapter.get(), 0 /* bluetooth_class */, kDeviceName, kDeviceAddress,
      false /* paired */, false /* connected */);
  EXPECT_CALL(*mock_device.get(), GetUUIDs)
      .WillRepeatedly(::testing::Return(
          std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)}));
  EXPECT_CALL(*mock_adapter, GetDevice(kDeviceAddress))
      .WillRepeatedly(::testing::Return(mock_device.get()));

  BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter.get());
  const auto device_id = FidoBleDevice::GetId(kDeviceAddress);
  discovery.Start();

  ::testing::InSequence sequence;
  EXPECT_CALL(observer,
              AuthenticatorAdded(&discovery, IdMatches(kDeviceAddress)));
  mock_adapter->NotifyDeviceChanged(mock_device.get());

  EXPECT_CALL(observer, AuthenticatorPairingModeChanged(&discovery, _));
  // Update device advertisement data so that it represents BLE device in
  // pairing mode.
  mock_device->UpdateAdvertisementData(
      0 /* rssi */, 1 << kLeLimitedDiscoverableModeBit,
      std::vector<BluetoothUUID>{BluetoothUUID(kFidoServiceUUID)},
      base::nullopt /* tx_power */, BluetoothDevice::ServiceDataMap(),
      BluetoothDevice::ManufacturerDataMap());
  mock_adapter->NotifyDeviceChanged(mock_device.get());
}

}  // namespace device
