// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_connection.h"

#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace proximity_auth {
namespace {

const char kDeviceName[] = "Device name";
const char kOtherDeviceName[] = "Other device name";

const char kBluetoothAddress[] = "11:22:33:44:55:66";
const char kOtherBluetoothAddress[] = "AA:BB:CC:DD:EE:FF";

const char kSerializedMessage[] = "Yarrr, this be a serialized message. Yarr!";
const int kSerializedMessageLength = strlen(kSerializedMessage);

const char kUuid[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEF";

const RemoteDevice kRemoteDevice = {kDeviceName, kBluetoothAddress};

const int kReceiveBufferSize = 6;
const char kReceiveBufferContents[] = "bytes";

// Create a buffer for testing received data.
scoped_refptr<net::IOBuffer> CreateReceiveBuffer() {
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kReceiveBufferSize);
  memcpy(buffer->data(), kReceiveBufferContents, kReceiveBufferSize);
  return buffer;
}

class MockBluetoothConnection : public BluetoothConnection {
 public:
  MockBluetoothConnection()
      : BluetoothConnection(kRemoteDevice, device::BluetoothUUID(kUuid)) {}

  // Bluetooth dependencies.
  typedef device::BluetoothDevice::ConnectToServiceCallback
      ConnectToServiceCallback;
  typedef device::BluetoothDevice::ConnectToServiceErrorCallback
      ConnectToServiceErrorCallback;
  MOCK_METHOD4(ConnectToService,
               void(device::BluetoothDevice* device,
                    const device::BluetoothUUID& uuid,
                    const ConnectToServiceCallback& callback,
                    const ConnectToServiceErrorCallback& error_callback));

  // Calls back into the parent Connection class.
  MOCK_METHOD1(SetStatusProxy, void(Status status));
  MOCK_METHOD1(OnBytesReceived, void(const std::string& bytes));
  MOCK_METHOD2(OnDidSendMessage,
               void(const WireMessage& message, bool success));

  virtual void SetStatus(Status status) OVERRIDE {
    SetStatusProxy(status);
    BluetoothConnection::SetStatus(status);
  }

  using BluetoothConnection::status;
  using BluetoothConnection::Connect;
  using BluetoothConnection::DeviceRemoved;
  using BluetoothConnection::Disconnect;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothConnection);
};

class TestWireMessage : public WireMessage {
 public:
  TestWireMessage() : WireMessage("permit id", "payload") {}
  virtual ~TestWireMessage() {}

  virtual std::string Serialize() const OVERRIDE { return kSerializedMessage; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWireMessage);
};

}  // namespace

class ProximityAuthBluetoothConnectionTest : public testing::Test {
 public:
  ProximityAuthBluetoothConnectionTest()
      : adapter_(new device::MockBluetoothAdapter),
        device_(adapter_.get(), 0, kDeviceName, kBluetoothAddress, true, true),
        socket_(new StrictMock<device::MockBluetoothSocket>),
        uuid_(kUuid) {
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    // Suppress uninteresting Gmock call warnings.
    EXPECT_CALL(*adapter_, GetDevice(_)).Times(AnyNumber());
  }

  // Transition the connection into an in-progress state.
  void BeginConnecting(MockBluetoothConnection* connection) {
    EXPECT_EQ(Connection::DISCONNECTED, connection->status());

    ON_CALL(*adapter_, GetDevice(_)).WillByDefault(Return(&device_));
    EXPECT_CALL(*connection, SetStatusProxy(Connection::IN_PROGRESS));
    EXPECT_CALL(*adapter_, AddObserver(connection));
    EXPECT_CALL(*connection, ConnectToService(&device_, uuid_, _, _));
    connection->Connect();

    EXPECT_EQ(Connection::IN_PROGRESS, connection->status());
  }

  // Transition the connection into a connected state.
  // Saves the success and error callbacks passed into OnReceive(), which can be
  // accessed via receive_callback() and receive_success_callback().
  void Connect(MockBluetoothConnection* connection) {
    EXPECT_EQ(Connection::DISCONNECTED, connection->status());

    device::BluetoothDevice::ConnectToServiceCallback callback;
    ON_CALL(*adapter_, GetDevice(_)).WillByDefault(Return(&device_));
    EXPECT_CALL(*connection, SetStatusProxy(Connection::IN_PROGRESS));
    EXPECT_CALL(*adapter_, AddObserver(connection));
    EXPECT_CALL(*connection, ConnectToService(_, _, _, _))
        .WillOnce(SaveArg<2>(&callback));
    connection->Connect();
    ASSERT_FALSE(callback.is_null());

    EXPECT_CALL(*connection, SetStatusProxy(Connection::CONNECTED));
    EXPECT_CALL(*socket_, Receive(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&receive_callback_),
                        SaveArg<2>(&receive_error_callback_)));
    callback.Run(socket_);

    EXPECT_EQ(Connection::CONNECTED, connection->status());
  }

  device::BluetoothSocket::ReceiveCompletionCallback* receive_callback() {
    return &receive_callback_;
  }
  device::BluetoothSocket::ReceiveErrorCompletionCallback*
  receive_error_callback() {
    return &receive_error_callback_;
  }

 protected:
  // Mocks used for verifying interactions with the Bluetooth subsystem.
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  NiceMock<device::MockBluetoothDevice> device_;
  scoped_refptr<StrictMock<device::MockBluetoothSocket>> socket_;

  device::BluetoothUUID uuid_;

 private:
  base::MessageLoop message_loop_;

  device::BluetoothSocket::ReceiveCompletionCallback receive_callback_;
  device::BluetoothSocket::ReceiveErrorCompletionCallback
      receive_error_callback_;
};

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_ConnectionWasInProgress) {
  // Create an in-progress connection.
  StrictMock<MockBluetoothConnection> connection;
  BeginConnecting(&connection);

  // A second call to Connect() should be ignored.
  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  connection.Connect();

  // The connection cleans up after itself upon destruction.
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_ConnectionWasConnected) {
  // Create a connected connection.
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  // A second call to Connect() should be ignored.
  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  connection.Connect();

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_NoBluetoothAdapter) {
  // Some platforms do not support Bluetooth. This test is only meaningful on
  // those platforms.
  adapter_ = NULL;
  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
    return;

  StrictMock<MockBluetoothConnection> connection;
  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  connection.Connect();
}

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_DeviceMissing) {
  StrictMock<MockBluetoothConnection> connection;

  ON_CALL(*adapter_, GetDevice(_))
      .WillByDefault(Return(static_cast<device::BluetoothDevice*>(NULL)));
  EXPECT_CALL(connection, SetStatusProxy(Connection::IN_PROGRESS));
  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  connection.Connect();
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_DeviceRemovedWhileConnecting) {
  // Create an in-progress connection.
  StrictMock<MockBluetoothConnection> connection;
  BeginConnecting(&connection);

  // Remove the device while the connection is in-progress. This should cause
  // the connection to disconnect.
  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  connection.DeviceRemoved(adapter_.get(), &device_);
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_OtherDeviceRemovedWhileConnecting) {
  // Create an in-progress connection.
  StrictMock<MockBluetoothConnection> connection;
  BeginConnecting(&connection);

  // Remove a device other than the one that is being connected to. This should
  // not have any effect on the connection.
  NiceMock<device::MockBluetoothDevice> other_device(
      adapter_.get(), 0, kOtherDeviceName, kOtherBluetoothAddress, true, true);
  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  connection.DeviceRemoved(adapter_.get(), &other_device);

  // The connection removes itself as an observer when it is destroyed.
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_ConnectionFails) {
  StrictMock<MockBluetoothConnection> connection;

  device::BluetoothDevice::ConnectToServiceErrorCallback error_callback;
  ON_CALL(*adapter_, GetDevice(_)).WillByDefault(Return(&device_));
  EXPECT_CALL(connection, SetStatusProxy(Connection::IN_PROGRESS));
  EXPECT_CALL(*adapter_, AddObserver(&connection));
  EXPECT_CALL(connection, ConnectToService(&device_, uuid_, _, _))
      .WillOnce(SaveArg<3>(&error_callback));
  connection.Connect();
  ASSERT_FALSE(error_callback.is_null());

  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  error_callback.Run("super descriptive error message");
}

TEST_F(ProximityAuthBluetoothConnectionTest, Connect_ConnectionSucceeds) {
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_ConnectionSucceeds_ThenDeviceRemoved) {
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  connection.DeviceRemoved(adapter_.get(), &device_);
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_ConnectionSucceeds_ReceiveData) {
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);
  ASSERT_TRUE(receive_callback() && !receive_callback()->is_null());

  // Receive some data. Once complete, the connection should re-register to be
  // ready receive more data.
  scoped_refptr<net::IOBuffer> buffer = CreateReceiveBuffer();
  EXPECT_CALL(
      connection,
      OnBytesReceived(std::string(kReceiveBufferContents, kReceiveBufferSize)));
  EXPECT_CALL(*socket_, Receive(_, _, _));
  receive_callback()->Run(kReceiveBufferSize, buffer);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_ConnectionSucceeds_ReceiveDataAfterReceiveError) {
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);
  ASSERT_TRUE(receive_error_callback() && !receive_error_callback()->is_null());

  // Simulate an error while receiving data. The connection should re-register
  // to be ready receive more data despite the error.
  device::BluetoothSocket::ReceiveCompletionCallback receive_callback;
  EXPECT_CALL(*socket_, Receive(_, _, _))
      .WillOnce(SaveArg<1>(&receive_callback));
  receive_error_callback()->Run(device::BluetoothSocket::kSystemError,
                                "The system is down. They're taking over!");
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  ASSERT_FALSE(receive_callback.is_null());

  // Receive some data.
  scoped_refptr<net::IOBuffer> buffer = CreateReceiveBuffer();
  EXPECT_CALL(
      connection,
      OnBytesReceived(std::string(kReceiveBufferContents, kReceiveBufferSize)));
  EXPECT_CALL(*socket_, Receive(_, _, _));
  receive_callback.Run(kReceiveBufferSize, buffer);
  base::RunLoop run_loop2;
  run_loop2.RunUntilIdle();

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Disconnect_ConnectionWasAlreadyDisconnected) {
  StrictMock<MockBluetoothConnection> connection;
  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  connection.Disconnect();
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Disconnect_ConnectionWasInProgress) {
  // Create an in-progress connection.
  StrictMock<MockBluetoothConnection> connection;
  BeginConnecting(&connection);

  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  connection.Disconnect();
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Disconnect_ConnectionWasConnected) {
  // Create a connected connection.
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  connection.Disconnect();
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       Connect_ThenDisconnectWhileInProgress_ThenBackingConnectionSucceeds) {
  StrictMock<MockBluetoothConnection> connection;
  device::BluetoothDevice::ConnectToServiceCallback callback;
  ON_CALL(*adapter_, GetDevice(_)).WillByDefault(Return(&device_));
  EXPECT_CALL(connection, SetStatusProxy(Connection::IN_PROGRESS));
  EXPECT_CALL(*adapter_, AddObserver(&connection));
  EXPECT_CALL(connection, ConnectToService(&device_, uuid_, _, _))
      .WillOnce(SaveArg<2>(&callback));
  connection.Connect();
  ASSERT_FALSE(callback.is_null());

  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  connection.Disconnect();

  EXPECT_CALL(connection, SetStatusProxy(_)).Times(0);
  EXPECT_CALL(*socket_, Receive(_, _, _)).Times(0);
  callback.Run(socket_);
}

TEST_F(ProximityAuthBluetoothConnectionTest,
       SendMessage_SendsExpectedDataOverTheWire) {
  // Create a connected connection.
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  scoped_refptr<net::IOBuffer> buffer;
  scoped_ptr<TestWireMessage> wire_message(new TestWireMessage);
  EXPECT_CALL(*socket_, Send(_, kSerializedMessageLength, _, _))
      .WillOnce(SaveArg<0>(&buffer));
  connection.SendMessage(wire_message.PassAs<WireMessage>());
  ASSERT_TRUE(buffer.get());
  EXPECT_EQ(kSerializedMessage,
            std::string(buffer->data(), kSerializedMessageLength));

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest, SendMessage_Success) {
  // Create a connected connection.
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  scoped_ptr<TestWireMessage> wire_message(new TestWireMessage);
  // Ownership will be transfered below, so grab a reference here.
  TestWireMessage* expected_wire_message = wire_message.get();

  device::BluetoothSocket::SendCompletionCallback callback;
  EXPECT_CALL(*socket_, Send(_, _, _, _)).WillOnce(SaveArg<2>(&callback));
  connection.SendMessage(wire_message.PassAs<WireMessage>());
  ASSERT_FALSE(callback.is_null());

  EXPECT_CALL(connection, OnDidSendMessage(Ref(*expected_wire_message), true));
  callback.Run(kSerializedMessageLength);

  // The connection disconnects and unregisters as an observer upon destruction.
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
}

TEST_F(ProximityAuthBluetoothConnectionTest, SendMessage_Failure) {
  // Create a connected connection.
  StrictMock<MockBluetoothConnection> connection;
  Connect(&connection);

  scoped_ptr<TestWireMessage> wire_message(new TestWireMessage);
  // Ownership will be transfered below, so grab a reference here.
  TestWireMessage* expected_wire_message = wire_message.get();

  device::BluetoothSocket::ErrorCompletionCallback error_callback;
  EXPECT_CALL(*socket_, Send(_, _, _, _)).WillOnce(SaveArg<3>(&error_callback));
  connection.SendMessage(wire_message.PassAs<WireMessage>());

  ASSERT_FALSE(error_callback.is_null());
  EXPECT_CALL(connection, OnDidSendMessage(Ref(*expected_wire_message), false));
  EXPECT_CALL(connection, SetStatusProxy(Connection::DISCONNECTED));
  EXPECT_CALL(*socket_, Disconnect(_));
  EXPECT_CALL(*adapter_, RemoveObserver(&connection));
  error_callback.Run("The most helpful of error messages");
}

}  // namespace proximity_auth
