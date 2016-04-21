// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection_finder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_device_whitelist.h"
#include "components/proximity_auth/fake_connection.h"
#include "components/proximity_auth/proximity_auth_test_util.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::SaveArg;

namespace proximity_auth {
namespace {

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kOtherUUID[] = "AAAAAAAA-AAAA-AAAA-AAAA-D15EA5EBEEEF";

const int kMaxNumberOfAttempts = 2;

class MockBluetoothLowEnergyDeviceWhitelist
    : public BluetoothLowEnergyDeviceWhitelist {
 public:
  MockBluetoothLowEnergyDeviceWhitelist()
      : BluetoothLowEnergyDeviceWhitelist(nullptr) {}
  ~MockBluetoothLowEnergyDeviceWhitelist() override {}

  MOCK_CONST_METHOD1(HasDeviceWithAddress, bool(const std::string&));
};

class MockBluetoothLowEnergyConnectionFinder
    : public BluetoothLowEnergyConnectionFinder {
 public:
  MockBluetoothLowEnergyConnectionFinder(
      const BluetoothLowEnergyDeviceWhitelist* device_whitelist,
      FinderStrategy finder_strategy)
      : BluetoothLowEnergyConnectionFinder(CreateLERemoteDeviceForTest(),
                                           kServiceUUID,
                                           finder_strategy,
                                           device_whitelist,
                                           nullptr,
                                           kMaxNumberOfAttempts) {}

  ~MockBluetoothLowEnergyConnectionFinder() override {}

  // Mock methods don't support return type std::unique_ptr<>. This is a
  // possible workaround: mock a proxy method to be called by the target
  // overridden method (CreateConnection).
  MOCK_METHOD0(CreateConnectionProxy, Connection*());

  // Creates a mock connection and sets an expectation that the mock connection
  // finder's CreateConnection() method will be called and will return the
  // created connection. Returns a reference to the created connection.
  // NOTE: The returned connection's lifetime is managed by the connection
  // finder.
  FakeConnection* ExpectCreateConnection() {
    std::unique_ptr<FakeConnection> connection(
        new FakeConnection(CreateLERemoteDeviceForTest()));
    FakeConnection* connection_alias = connection.get();
    EXPECT_CALL(*this, CreateConnectionProxy())
        .WillOnce(Return(connection.release()));
    return connection_alias;
  }

  MOCK_METHOD0(CloseGattConnectionProxy, void(void));

 protected:
  std::unique_ptr<Connection> CreateConnection(
      const std::string& device_address) override {
    return base::WrapUnique(CreateConnectionProxy());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothLowEnergyConnectionFinder);
};

}  // namespace

class ProximityAuthBluetoothLowEnergyConnectionFinderTest
    : public testing::Test {
 protected:
  ProximityAuthBluetoothLowEnergyConnectionFinderTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        connection_callback_(
            base::Bind(&ProximityAuthBluetoothLowEnergyConnectionFinderTest::
                           OnConnectionFound,
                       base::Unretained(this))),
        device_(new NiceMock<device::MockBluetoothDevice>(
            adapter_.get(),
            0,
            kTestRemoteDeviceName,
            kTestRemoteDeviceBluetoothAddress,
            false,
            false)),
        device_whitelist_(new MockBluetoothLowEnergyDeviceWhitelist()),
        last_discovery_session_alias_(nullptr) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::vector<const device::BluetoothDevice*> devices;
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));

    ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));

    ON_CALL(*device_whitelist_, HasDeviceWithAddress(_))
        .WillByDefault(Return(false));
  }

  void OnConnectionFound(std::unique_ptr<Connection> connection) {
    last_found_connection_ = std::move(connection);
  }

  void FindAndExpectStartDiscovery(
      BluetoothLowEnergyConnectionFinder& connection_finder) {
    device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
    std::unique_ptr<device::MockBluetoothDiscoverySession> discovery_session(
        new NiceMock<device::MockBluetoothDiscoverySession>());
    last_discovery_session_alias_ = discovery_session.get();

    // Starting a discovery session. StartDiscoveryWithFilterRaw is a proxy for
    // StartDiscoveryWithFilter.
    EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
        .WillOnce(SaveArg<1>(&discovery_callback));
    EXPECT_CALL(*adapter_, AddObserver(_));
    ON_CALL(*last_discovery_session_alias_, IsActive())
        .WillByDefault(Return(true));
    connection_finder.Find(connection_callback_);
    ASSERT_FALSE(discovery_callback.is_null());
    discovery_callback.Run(std::move(discovery_session));
  }

  void ExpectRemoveObserver() {
    EXPECT_CALL(*adapter_, RemoveObserver(_)).Times(AtLeast(1));
  }

  // Prepare |device_| with |uuid|.
  void PrepareDevice(const std::string& uuid,
                     const std::string& address,
                     bool paired) {
    std::vector<device::BluetoothUUID> uuids;
    uuids.push_back(device::BluetoothUUID(uuid));
    ON_CALL(*device_, GetUUIDs()).WillByDefault(Return(uuids));
    ON_CALL(*device_, GetAddress()).WillByDefault(Return(address));
    ON_CALL(*device_, IsPaired()).WillByDefault(Return(paired));
  }

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  ConnectionFinder::ConnectionCallback connection_callback_;
  std::unique_ptr<device::MockBluetoothDevice> device_;
  std::unique_ptr<Connection> last_found_connection_;
  std::unique_ptr<MockBluetoothLowEnergyDeviceWhitelist> device_whitelist_;
  device::MockBluetoothDiscoverySession* last_discovery_session_alias_;

 private:
  base::MessageLoop message_loop_;
};

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       ConstructAndDestroyDoesntCrash) {
  // Destroying a BluetoothConnectionFinder for which Find() has not been called
  // should not crash.
  BluetoothLowEnergyConnectionFinder connection_finder(
      CreateLERemoteDeviceForTest(), kServiceUUID,
      BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE,
      device_whitelist_.get(), nullptr, kMaxNumberOfAttempts);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_StartsDiscoverySession) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      CreateLERemoteDeviceForTest(), kServiceUUID,
      BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE,
      device_whitelist_.get(), nullptr, kMaxNumberOfAttempts);

  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _));
  EXPECT_CALL(*adapter_, AddObserver(_));
  connection_finder.Find(connection_callback_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_StopsDiscoverySessionBeforeDestroying) {
  BluetoothLowEnergyConnectionFinder connection_finder(
      CreateLERemoteDeviceForTest(), kServiceUUID,
      BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE,
      device_whitelist_.get(), nullptr, kMaxNumberOfAttempts);

  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  std::unique_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  device::MockBluetoothDiscoverySession* discovery_session_alias =
      discovery_session.get();

  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));
  ON_CALL(*discovery_session_alias, IsActive()).WillByDefault(Return(true));
  EXPECT_CALL(*adapter_, AddObserver(_));
  connection_finder.Find(connection_callback_);

  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(std::move(discovery_session));

  EXPECT_CALL(*adapter_, RemoveObserver(_));
}

// TODO(sacomoto): Remove it when ProximityAuthBleSystem is not needed anymore.
TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesConnectionWhenWhitelistedDeviceIsAdded) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      device_whitelist_.get(),
      BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);
  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  std::vector<device::BluetoothUUID> uuids;
  ON_CALL(*device_, GetUUIDs()).WillByDefault(Return(uuids));
  ON_CALL(*device_, IsPaired()).WillByDefault(Return(true));
  ON_CALL(*device_whitelist_, HasDeviceWithAddress(_))
      .WillByDefault(Return(true));

  connection_finder.ExpectCreateConnection();
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesConnectionWhenRightDeviceIsAdded_NoPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);

  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, false);
  ON_CALL(*device_, GetName())
      .WillByDefault(Return(base::UTF8ToUTF16(kTestRemoteDeviceName)));

  connection_finder.ExpectCreateConnection();
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_DoesntCreatesConnectionWhenWrongDeviceIsAdded_NoPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);

  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kOtherUUID, kTestRemoteDeviceBluetoothAddress, false);
  ON_CALL(*device_, GetName())
      .WillByDefault(Return(base::UTF8ToUTF16("Other name")));

  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesConnectionWhenRightDeviceIsAdded_HasPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);

  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  connection_finder.ExpectCreateConnection();
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_DoesntCreateConnectionWhenWrongDeviceIsAdded_HasPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);
  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kOtherUUID, "", true);
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesConnectionWhenRightDeviceIsChanged_HasPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);

  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  connection_finder.ExpectCreateConnection();
  connection_finder.DeviceChanged(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_DoesntCreateConnectionWhenWrongDeviceIsChanged_HasPublicAddress) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);

  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  PrepareDevice(kOtherUUID, "", true);
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.DeviceChanged(adapter_.get(), device_.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_CreatesOnlyOneConnection) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_ANY_DEVICE);
  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  // Prepare to add |device_|.
  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);

  // Prepare to add |other_device|.
  NiceMock<device::MockBluetoothDevice> other_device(
      adapter_.get(), 0, kTestRemoteDeviceName,
      kTestRemoteDeviceBluetoothAddress, false, false);
  std::vector<device::BluetoothUUID> uuids;
  uuids.push_back(device::BluetoothUUID(kServiceUUID));
  ON_CALL(other_device, GetAddress())
      .WillByDefault(Return(kTestRemoteDeviceBluetoothAddress));
  ON_CALL(other_device, IsPaired()).WillByDefault(Return(true));
  ON_CALL(other_device, GetUUIDs()).WillByDefault((Return(uuids)));

  // Only one connection should be created.
  connection_finder.ExpectCreateConnection();

  // Add the devices.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  connection_finder.DeviceAdded(adapter_.get(), &other_device);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_ConnectionSucceeds_WithRemoteDevice) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE);
  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);
  ExpectRemoveObserver();

  // Finding and creating a connection to the right device.
  FakeConnection* connection = connection_finder.ExpectCreateConnection();
  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  connection_finder.DeviceAdded(adapter_.get(), device_.get());

  // Creating a connection.
  base::RunLoop run_loop;
  EXPECT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
  run_loop.RunUntilIdle();
  EXPECT_TRUE(last_found_connection_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_ConnectionFails_RestartDiscoveryAndConnectionSucceeds) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE);

  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);

  // Preparing to create a GATT connection to the right device.
  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  FakeConnection* connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());
  ASSERT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);

  // Preparing to restart the discovery session.
  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  std::vector<const device::BluetoothDevice*> devices;
  ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));

  // Connection fails.
  {
    base::RunLoop run_loop;
    connection->SetStatus(Connection::DISCONNECTED);
    run_loop.RunUntilIdle();
  }

  // Restarting the discovery session.
  std::unique_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  last_discovery_session_alias_ = discovery_session.get();
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(true));
  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(std::move(discovery_session));

  // Preparing to create a GATT connection to the right device.
  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());

  // Completing the connection.
  {
    base::RunLoop run_loop;
    EXPECT_FALSE(last_found_connection_);
    connection->SetStatus(Connection::IN_PROGRESS);
    connection->SetStatus(Connection::CONNECTED);
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(last_found_connection_);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionFinderTest,
       Find_AdapterRemoved_RestartDiscoveryAndConnectionSucceeds) {
  StrictMock<MockBluetoothLowEnergyConnectionFinder> connection_finder(
      nullptr, BluetoothLowEnergyConnectionFinder::FIND_PAIRED_DEVICE);

  // Starting discovery.
  FindAndExpectStartDiscovery(connection_finder);

  // Removing the adapter.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(false));
  connection_finder.AdapterPoweredChanged(adapter_.get(), false);
  connection_finder.AdapterPresentChanged(adapter_.get(), false);

  // Adding the adapter.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));

  device::BluetoothAdapter::DiscoverySessionCallback discovery_callback;
  std::unique_ptr<device::MockBluetoothDiscoverySession> discovery_session(
      new NiceMock<device::MockBluetoothDiscoverySession>());
  last_discovery_session_alias_ = discovery_session.get();

  // Restarting the discovery session.
  EXPECT_CALL(*adapter_, StartDiscoverySessionWithFilterRaw(_, _, _))
      .WillOnce(SaveArg<1>(&discovery_callback));
  connection_finder.AdapterPresentChanged(adapter_.get(), true);
  connection_finder.AdapterPoweredChanged(adapter_.get(), true);
  ON_CALL(*last_discovery_session_alias_, IsActive())
      .WillByDefault(Return(true));

  ASSERT_FALSE(discovery_callback.is_null());
  discovery_callback.Run(std::move(discovery_session));

  // Preparing to create a GATT connection to the right device.
  PrepareDevice(kServiceUUID, kTestRemoteDeviceBluetoothAddress, true);
  FakeConnection* connection = connection_finder.ExpectCreateConnection();

  // Trying to create a connection.
  connection_finder.DeviceAdded(adapter_.get(), device_.get());

  // Completing the connection.
  base::RunLoop run_loop;
  ASSERT_FALSE(last_found_connection_);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
  run_loop.RunUntilIdle();
  EXPECT_TRUE(last_found_connection_);
}

}  // namespace proximity_auth
