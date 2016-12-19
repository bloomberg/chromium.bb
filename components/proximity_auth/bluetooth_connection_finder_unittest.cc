// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_connection_finder.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/cryptauth/cryptauth_test_util.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {
namespace {

const char kUuid[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEF";

class MockConnection : public cryptauth::Connection {
 public:
  MockConnection()
      : Connection(cryptauth::CreateClassicRemoteDeviceForTest()),
        do_not_destroy_(false) {}
  ~MockConnection() override { EXPECT_FALSE(do_not_destroy_); }

  MOCK_METHOD0(Connect, void());

  void SetStatus(cryptauth::Connection::Status status) {
    // This object should not be destroyed after setting the status and calling
    // observers.
    do_not_destroy_ = true;
    cryptauth::Connection::SetStatus(status);
    do_not_destroy_ = false;
  }

 private:
  void Disconnect() override {}
  void SendMessageImpl(
      std::unique_ptr<cryptauth::WireMessage> message) override {}

  // If true, we do not expect |this| object to be destroyed until this value is
  // toggled back to false.
  bool do_not_destroy_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class MockBluetoothConnectionFinder : public BluetoothConnectionFinder {
 public:
  MockBluetoothConnectionFinder()
      : BluetoothConnectionFinder(cryptauth::CreateClassicRemoteDeviceForTest(),
                                  device::BluetoothUUID(kUuid),
                                  base::TimeDelta()) {}
  ~MockBluetoothConnectionFinder() override {}

  MOCK_METHOD0(CreateConnectionProxy, cryptauth::Connection*());

  // Creates a mock connection and sets an expectation that the mock connection
  // finder's CreateConnection() method will be called and will return the
  // created connection. Returns a reference to the created connection.
  // NOTE: The returned connection's lifetime is managed by the connection
  // finder.
  MockConnection* ExpectCreateConnection() {
    std::unique_ptr<MockConnection> connection(new NiceMock<MockConnection>());
    MockConnection* connection_alias = connection.get();
    EXPECT_CALL(*this, CreateConnectionProxy())
        .WillOnce(Return(connection.release()));
    return connection_alias;
  }

  using BluetoothConnectionFinder::AdapterPresentChanged;
  using BluetoothConnectionFinder::AdapterPoweredChanged;

  void ClearSeekCallbacks() {
    seek_callback_ = base::Closure();
    seek_error_callback_ = bluetooth_util::ErrorCallback();
  }

  const base::Closure& seek_callback() { return seek_callback_; }
  const bluetooth_util::ErrorCallback& seek_error_callback() {
    return seek_error_callback_;
  }

 protected:
  // BluetoothConnectionFinder:
  std::unique_ptr<cryptauth::Connection> CreateConnection() override {
    return base::WrapUnique(CreateConnectionProxy());
  }

  void SeekDeviceByAddress(
      const std::string& bluetooth_address,
      const base::Closure& callback,
      const bluetooth_util::ErrorCallback& error_callback) override {
    EXPECT_EQ(cryptauth::kTestRemoteDeviceBluetoothAddress, bluetooth_address);
    seek_callback_ = callback;
    seek_error_callback_ = error_callback;
  }

 private:
  base::Closure seek_callback_;
  bluetooth_util::ErrorCallback seek_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockBluetoothConnectionFinder);
};

}  // namespace

class ProximityAuthBluetoothConnectionFinderTest : public testing::Test {
 protected:
  ProximityAuthBluetoothConnectionFinderTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        bluetooth_device_(new NiceMock<device::MockBluetoothDevice>(
            adapter_.get(),
            static_cast<uint32_t>(device::BluetoothDeviceType::PHONE),
            cryptauth::kTestRemoteDeviceName,
            cryptauth::kTestRemoteDeviceBluetoothAddress,
            true,
            false)),
        connection_callback_(base::Bind(
            &ProximityAuthBluetoothConnectionFinderTest::OnConnectionFound,
            base::Unretained(this))) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    // By default, configure the environment to allow polling. Individual tests
    // can override this as needed.
    ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));

    // By default, the remote device is known to |adapter_| so
    // |SeekDeviceByAddress()| will not be called.
    ON_CALL(*adapter_, GetDevice(cryptauth::kTestRemoteDeviceBluetoothAddress))
        .WillByDefault(Return(bluetooth_device_.get()));
  }

  MOCK_METHOD1(OnConnectionFoundProxy, void(cryptauth::Connection* connection));
  void OnConnectionFound(std::unique_ptr<cryptauth::Connection> connection) {
    OnConnectionFoundProxy(connection.get());
    last_found_connection_ = std::move(connection);
  }

  // Starts |connection_finder_|. If |expect_connection| is true, then we set an
  // expectation that an in-progress connection will be created and returned.
  MockConnection* StartConnectionFinder(bool expect_connection) {
    MockConnection* connection = nullptr;
    if (expect_connection)
      connection = connection_finder_.ExpectCreateConnection();
    connection_finder_.Find(connection_callback_);
    return connection;
  }

  // Given an in-progress |connection| returned by |StartConnectionFinder()|,
  // simulate it transitioning to the CONNECTED state.
  void SimulateDeviceConnection(MockConnection* connection) {
    connection->SetStatus(cryptauth::Connection::IN_PROGRESS);
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnConnectionFoundProxy(_));
    connection->SetStatus(cryptauth::Connection::CONNECTED);
    run_loop.RunUntilIdle();
  }

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  StrictMock<MockBluetoothConnectionFinder> connection_finder_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_;
  cryptauth::ConnectionFinder::ConnectionCallback connection_callback_;

 private:
  // Save a pointer to the last found connection, to extend its lifetime.
  std::unique_ptr<cryptauth::Connection> last_found_connection_;

  base::MessageLoop message_loop_;
};

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       ConstructAndDestroyDoesntCrash) {
  // Destroying a BluetoothConnectionFinder for which Find() has not been called
  // should not crash.
  BluetoothConnectionFinder connection_finder(
      cryptauth::CreateClassicRemoteDeviceForTest(),
      device::BluetoothUUID(kUuid), base::TimeDelta::FromMilliseconds(1));
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_NoBluetoothAdapter) {
  // Some platforms do not support Bluetooth. This test is only meaningful on
  // those platforms.
  adapter_ = NULL;
  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    return;

  // The StrictMock will verify that no connection is created.
  StartConnectionFinder(false);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_BluetoothAdapterNotPresent) {
  // The StrictMock will verify that no connection is created.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  StartConnectionFinder(false);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_BluetoothAdapterNotPowered) {
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  // The StrictMock will verify that no connection is created.
  StartConnectionFinder(false);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_ConnectionSucceeds) {
  MockConnection* connection = StartConnectionFinder(true);
  SimulateDeviceConnection(connection);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionSucceeds_UnregistersAsObserver) {
  MockConnection* connection = StartConnectionFinder(true);
  SimulateDeviceConnection(connection);

  // If for some reason the connection sends more status updates, they should
  // be ignored.
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnConnectionFoundProxy(_)).Times(0);
  connection->SetStatus(cryptauth::Connection::IN_PROGRESS);
  connection->SetStatus(cryptauth::Connection::CONNECTED);
  run_loop.RunUntilIdle();
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionFails_PostsTaskToPollAgain) {
  MockConnection* connection = StartConnectionFinder(true);

  // Simulate a connection that fails to connect.
  connection->SetStatus(cryptauth::Connection::IN_PROGRESS);
  connection->SetStatus(cryptauth::Connection::DISCONNECTED);

  // A task should have been posted to poll again.
  base::RunLoop run_loop;
  connection_finder_.ExpectCreateConnection();
  run_loop.RunUntilIdle();
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_PollsOnAdapterPresent) {
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  EXPECT_CALL(connection_finder_, CreateConnectionProxy()).Times(0);
  connection_finder_.Find(connection_callback_);

  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
  connection_finder_.ExpectCreateConnection();
  connection_finder_.AdapterPresentChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_PollsOnAdapterPowered) {
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  EXPECT_CALL(connection_finder_, CreateConnectionProxy()).Times(0);
  connection_finder_.Find(connection_callback_);

  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));
  connection_finder_.ExpectCreateConnection();
  connection_finder_.AdapterPoweredChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_DoesNotPollIfConnectionPending) {
  MockConnection* connection = StartConnectionFinder(true);

  connection->SetStatus(cryptauth::Connection::IN_PROGRESS);

  // At this point, there is a pending connection in progress. Hence, an event
  // that would normally trigger a new polling iteration should not do so now,
  // because the delay interval between successive polling attempts has not yet
  // expired.
  EXPECT_CALL(connection_finder_, CreateConnectionProxy()).Times(0);
  connection_finder_.AdapterPresentChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionFails_PostsTaskToPollAgain_PollWaitsForTask) {
  MockConnection* connection = StartConnectionFinder(true);

  connection->SetStatus(cryptauth::Connection::IN_PROGRESS);
  connection->SetStatus(cryptauth::Connection::DISCONNECTED);

  // At this point, there is a pending poll scheduled. Hence, an event that
  // would normally trigger a new polling iteration should not do so now,
  // because the delay interval between successive polling attempts has not yet
  // expired.
  EXPECT_CALL(connection_finder_, CreateConnectionProxy()).Times(0);
  connection_finder_.AdapterPresentChanged(adapter_.get(), true);

  // Now, allow the pending task to run, but fail early, so that no new task is
  // posted.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));

  // Now that there is no pending task, events should once again trigger new
  // polling iterations.
  connection_finder_.ExpectCreateConnection();
  connection_finder_.AdapterPresentChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_DeviceNotKnown_SeekDeviceSucceeds) {
  // If the BluetoothDevice is not known by the adapter, |connection_finder|
  // will call SeekDeviceByAddress() first to make it known.
  ON_CALL(*adapter_, GetDevice(cryptauth::kTestRemoteDeviceBluetoothAddress))
      .WillByDefault(Return(nullptr));
  connection_finder_.Find(connection_callback_);
  ASSERT_FALSE(connection_finder_.seek_callback().is_null());
  EXPECT_FALSE(connection_finder_.seek_error_callback().is_null());

  // After seeking is successful, the normal flow should resume.
  ON_CALL(*adapter_, GetDevice(cryptauth::kTestRemoteDeviceBluetoothAddress))
      .WillByDefault(Return(bluetooth_device_.get()));
  MockConnection* connection = connection_finder_.ExpectCreateConnection();
  connection_finder_.seek_callback().Run();
  SimulateDeviceConnection(connection);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_DeviceNotKnown_SeekDeviceFailThenSucceeds) {
  // If the BluetoothDevice is not known by the adapter, |connection_finder|
  // will call SeekDeviceByAddress() first to make it known.
  ON_CALL(*adapter_, GetDevice(cryptauth::kTestRemoteDeviceBluetoothAddress))
      .WillByDefault(Return(nullptr));
  connection_finder_.Find(connection_callback_);
  EXPECT_FALSE(connection_finder_.seek_callback().is_null());
  ASSERT_FALSE(connection_finder_.seek_error_callback().is_null());

  // If the seek fails, then |connection_finder| will post a delayed poll to
  // reattempt the seek.
  connection_finder_.seek_error_callback().Run("Seek failed for test.");
  connection_finder_.ClearSeekCallbacks();
  EXPECT_TRUE(connection_finder_.seek_callback().is_null());
  EXPECT_TRUE(connection_finder_.seek_error_callback().is_null());

  // Check that seek is reattempted.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ASSERT_FALSE(connection_finder_.seek_callback().is_null());
  EXPECT_FALSE(connection_finder_.seek_error_callback().is_null());

  // Successfully connect to the Bluetooth device.
  ON_CALL(*adapter_, GetDevice(cryptauth::kTestRemoteDeviceBluetoothAddress))
      .WillByDefault(Return(bluetooth_device_.get()));
  MockConnection* connection = connection_finder_.ExpectCreateConnection();
  connection_finder_.seek_callback().Run();
  SimulateDeviceConnection(connection);
}

}  // namespace proximity_auth
