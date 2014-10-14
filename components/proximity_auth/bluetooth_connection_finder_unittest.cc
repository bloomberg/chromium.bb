// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_connection_finder.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace proximity_auth {
namespace {

const char kDeviceName[] = "Device name";
const char kBluetoothAddress[] = "11:22:33:44:55:66";
const RemoteDevice kRemoteDevice = {kDeviceName, kBluetoothAddress};

const char kUuid[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEF";

class MockConnection : public Connection {
 public:
  MockConnection() : Connection(kRemoteDevice) {}
  virtual ~MockConnection() {}

  MOCK_METHOD0(Connect, void());

  using Connection::SetStatus;

 private:
  void Disconnect() {}
  void SendMessageImpl(scoped_ptr<WireMessage> message) {}

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class MockBluetoothConnectionFinder : public BluetoothConnectionFinder {
 public:
  MockBluetoothConnectionFinder()
      : BluetoothConnectionFinder(kRemoteDevice,
                                  device::BluetoothUUID(kUuid),
                                  base::TimeDelta()) {}
  virtual ~MockBluetoothConnectionFinder() {}

  MOCK_METHOD0(CreateConnectionProxy, Connection*());

  // Creates a mock connection and sets an expectation that the mock connection
  // finder's CreateConnection() method will be called and will return the
  // created connection. Returns a reference to the created connection.
  // NOTE: The returned connection's lifetime is managed by the connection
  // finder.
  MockConnection* ExpectCreateConnection() {
    scoped_ptr<MockConnection> connection(new NiceMock<MockConnection>());
    MockConnection* connection_alias = connection.get();
    EXPECT_CALL(*this, CreateConnectionProxy())
        .WillOnce(Return(connection.release()));
    return connection_alias;
  }

  using BluetoothConnectionFinder::AdapterPresentChanged;
  using BluetoothConnectionFinder::AdapterPoweredChanged;

 protected:
  virtual scoped_ptr<Connection> CreateConnection() override {
    return make_scoped_ptr(CreateConnectionProxy());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothConnectionFinder);
};

}  // namespace

class ProximityAuthBluetoothConnectionFinderTest : public testing::Test {
 protected:
  ProximityAuthBluetoothConnectionFinderTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        connection_callback_(base::Bind(
            &ProximityAuthBluetoothConnectionFinderTest::OnConnectionFound,
            base::Unretained(this))) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    // By default, configure the environment to allow polling. Individual tests
    // can override this as needed.
    ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));
  }

  MOCK_METHOD1(OnConnectionFoundProxy, void(Connection* connection));
  void OnConnectionFound(scoped_ptr<Connection> connection) {
    OnConnectionFoundProxy(connection.get());
    last_found_connection_ = connection.Pass();
  }

  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  ConnectionFinder::ConnectionCallback connection_callback_;

 private:
  // Save a pointer to the last found connection, to extend its lifetime.
  scoped_ptr<Connection> last_found_connection_;

  base::MessageLoop message_loop_;
};

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       ConstructAndDestroyDoesntCrash) {
  // Destroying a BluetoothConnectionFinder for which Find() has not been called
  // should not crash.
  BluetoothConnectionFinder connection_finder(
      kRemoteDevice,
      device::BluetoothUUID(kUuid),
      base::TimeDelta::FromMilliseconds(1));
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_NoBluetoothAdapter) {
  // Some platforms do not support Bluetooth. This test is only meaningful on
  // those platforms.
  adapter_ = NULL;
  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    return;

  // The StrictMock will verify that no connection is created.
  StrictMock<MockBluetoothConnectionFinder> connection_finder;
  connection_finder.Find(connection_callback_);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_BluetoothAdapterNotPresent) {
  // The StrictMock will verify that no connection is created.
  StrictMock<MockBluetoothConnectionFinder> connection_finder;
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  connection_finder.Find(connection_callback_);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_BluetoothAdapterNotPowered) {
  // The StrictMock will verify that no connection is created.
  StrictMock<MockBluetoothConnectionFinder> connection_finder;
  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  connection_finder.Find(connection_callback_);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_ConnectionSucceeds) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  MockConnection* connection = connection_finder.ExpectCreateConnection();
  connection_finder.Find(connection_callback_);

  connection->SetStatus(Connection::IN_PROGRESS);

  EXPECT_CALL(*this, OnConnectionFoundProxy(_));
  connection->SetStatus(Connection::CONNECTED);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionSucceeds_UnregistersAsObserver) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  MockConnection* connection = connection_finder.ExpectCreateConnection();
  connection_finder.Find(connection_callback_);

  connection->SetStatus(Connection::IN_PROGRESS);

  EXPECT_CALL(*this, OnConnectionFoundProxy(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection_finder));
  connection->SetStatus(Connection::CONNECTED);

  // If for some reason the connection sends more status updates, they should be
  // ignored.
  EXPECT_CALL(*this, OnConnectionFoundProxy(_)).Times(0);
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::CONNECTED);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionFails_PostsTaskToPollAgain) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  MockConnection* connection = connection_finder.ExpectCreateConnection();
  connection_finder.Find(connection_callback_);

  // Simulate a connection that fails to connect.
  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::DISCONNECTED);

  // A task should have been posted to poll again.
  base::RunLoop run_loop;
  connection_finder.ExpectCreateConnection();
  run_loop.RunUntilIdle();
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_PollsOnAdapterPresent) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.Find(connection_callback_);

  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));
  connection_finder.ExpectCreateConnection();
  connection_finder.AdapterPresentChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest, Find_PollsOnAdapterPowered) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(false));
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.Find(connection_callback_);

  ON_CALL(*adapter_, IsPowered()).WillByDefault(Return(true));
  connection_finder.ExpectCreateConnection();
  connection_finder.AdapterPoweredChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_DoesNotPollIfConnectionPending) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  MockConnection* connection = connection_finder.ExpectCreateConnection();
  connection_finder.Find(connection_callback_);

  connection->SetStatus(Connection::IN_PROGRESS);

  // At this point, there is a pending connection in progress. Hence, an event
  // that would normally trigger a new polling iteration should not do so now,
  // because the delay interval between successive polling attempts has not yet
  // expired.
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.AdapterPresentChanged(adapter_.get(), true);
}

TEST_F(ProximityAuthBluetoothConnectionFinderTest,
       Find_ConnectionFails_PostsTaskToPollAgain_PollWaitsForTask) {
  StrictMock<MockBluetoothConnectionFinder> connection_finder;

  MockConnection* connection = connection_finder.ExpectCreateConnection();
  connection_finder.Find(connection_callback_);

  connection->SetStatus(Connection::IN_PROGRESS);
  connection->SetStatus(Connection::DISCONNECTED);

  // At this point, there is a pending poll scheduled. Hence, an event that
  // would normally trigger a new polling iteration should not do so now,
  // because the delay interval between successive polling attempts has not yet
  // expired.
  EXPECT_CALL(connection_finder, CreateConnectionProxy()).Times(0);
  connection_finder.AdapterPresentChanged(adapter_.get(), true);

  // Now, allow the pending task to run, but fail early, so that no new task is
  // posted.
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(false));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ON_CALL(*adapter_, IsPresent()).WillByDefault(Return(true));

  // Now that there is no pending task, events should once again trigger new
  // polling iterations.
  connection_finder.ExpectCreateConnection();
  connection_finder.AdapterPresentChanged(adapter_.get(), true);
}

}  // namespace proximity_auth
