// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_characteristics_finder.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
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

const char kDeviceName[] = "Device name";
const char kBluetoothAddress[] = "11:22:33:44:55:66";

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kToPeripheralCharUUID[] = "FBAE09F2-0482-11E5-8418-1697F925EC7B";
const char kFromPeripheralCharUUID[] = "5539ED10-0483-11E5-8418-1697F925EC7B";

const char kServiceID[] = "service id";
const char kToPeripheralCharID[] = "to peripheral char id";
const char kFromPeripheralCharID[] = "from peripheral char id";

const device::BluetoothGattCharacteristic::Properties
    kCharacteristicProperties =
        device::BluetoothGattCharacteristic::PROPERTY_BROADCAST |
        device::BluetoothGattCharacteristic::PROPERTY_READ |
        device::BluetoothGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
        device::BluetoothGattCharacteristic::PROPERTY_INDICATE;

const int kMaxNumberOfTries = 3;

class MockBluetoothLowEnergyCharacteristicsFinder
    : public BluetoothLowEnergyCharacteristicsFinder {
 public:
  MockBluetoothLowEnergyCharacteristicsFinder() {}
  ~MockBluetoothLowEnergyCharacteristicsFinder() override {}
};

class MockBluetoothLowEnergyConnection : public BluetoothLowEnergyConnection {
 public:
  MockBluetoothLowEnergyConnection(
      const RemoteDevice& remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      const device::BluetoothUUID to_peripheral_char_uuid,
      const device::BluetoothUUID from_peripheral_char_uuid,
      scoped_ptr<device::BluetoothGattConnection> gatt_connection,
      int max_number_of_write_attempts)
      : BluetoothLowEnergyConnection(remote_device,
                                     adapter,
                                     remote_service_uuid,
                                     to_peripheral_char_uuid,
                                     from_peripheral_char_uuid,
                                     gatt_connection.Pass(),
                                     max_number_of_write_attempts) {}

  ~MockBluetoothLowEnergyConnection() override {}

  MOCK_METHOD2(
      CreateCharacteristicsFinder,
      BluetoothLowEnergyCharacteristicsFinder*(
          const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
              success,
          const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback& error));

  MOCK_METHOD2(OnDidSendMessage,
               void(const WireMessage& message, bool success));
  MOCK_METHOD1(OnBytesReceived, void(const std::string& bytes));

  // Exposing inherited protected methods for testing.
  using BluetoothLowEnergyConnection::GattCharacteristicValueChanged;

  // Exposing inherited protected fields for testing.
  using BluetoothLowEnergyConnection::status;
  using BluetoothLowEnergyConnection::sub_status;
};

}  // namespace

class ProximityAuthBluetoothLowEnergyConnectionTest : public testing::Test {
 public:
  ProximityAuthBluetoothLowEnergyConnectionTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        remote_device_({kDeviceName, kBluetoothAddress}),
        service_uuid_(device::BluetoothUUID(kServiceUUID)),
        to_peripheral_char_uuid_(device::BluetoothUUID(kToPeripheralCharUUID)),
        from_peripheral_char_uuid_(
            device::BluetoothUUID(kFromPeripheralCharUUID)),
        gatt_connection_(new NiceMock<device::MockBluetoothGattConnection>(
            kBluetoothAddress)),
        gatt_connection_alias_(gatt_connection_.get()),
        notify_session_alias_(NULL) {}

  void SetUp() override {
    device_ = make_scoped_ptr(new NiceMock<device::MockBluetoothDevice>(
        adapter_.get(), 0, kDeviceName, kBluetoothAddress, false, false));

    service_ = make_scoped_ptr(new NiceMock<device::MockBluetoothGattService>(
        device_.get(), kServiceID, service_uuid_, true, false));
    to_peripheral_char_ =
        make_scoped_ptr(new NiceMock<device::MockBluetoothGattCharacteristic>(
            service_.get(), kToPeripheralCharID, to_peripheral_char_uuid_,
            false, kCharacteristicProperties,
            device::BluetoothGattCharacteristic::PERMISSION_NONE));

    from_peripheral_char_ =
        make_scoped_ptr(new NiceMock<device::MockBluetoothGattCharacteristic>(
            service_.get(), kFromPeripheralCharID, from_peripheral_char_uuid_,
            false, kCharacteristicProperties,
            device::BluetoothGattCharacteristic::PERMISSION_NONE));

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::vector<const device::BluetoothDevice*> devices;
    devices.push_back(device_.get());
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
    ON_CALL(*device_, GetGattService(kServiceID))
        .WillByDefault(Return(service_.get()));
    ON_CALL(*service_, GetCharacteristic(kFromPeripheralCharID))
        .WillByDefault(Return(from_peripheral_char_.get()));
    ON_CALL(*service_, GetCharacteristic(kToPeripheralCharID))
        .WillByDefault(Return(to_peripheral_char_.get()));
  }

  // Creates a BluetoothLowEnergyConnection and verifies it's in DISCONNECTED
  // state.
  scoped_ptr<MockBluetoothLowEnergyConnection> CreateConnection() {
    EXPECT_CALL(*adapter_, AddObserver(_));
    EXPECT_CALL(*adapter_, RemoveObserver(_));

    scoped_ptr<MockBluetoothLowEnergyConnection> connection(
        new MockBluetoothLowEnergyConnection(
            remote_device_, adapter_, service_uuid_, to_peripheral_char_uuid_,
            from_peripheral_char_uuid_, gatt_connection_.Pass(),
            kMaxNumberOfTries));

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::DISCONNECTED);

    return connection.Pass();
  }

  // Transitions |connection| from DISCONNECTED to WAITING_CHARACTERISTICS
  // state, using an existing GATT connection.
  void ConnectWithExistingGattConnection(
      MockBluetoothLowEnergyConnection* connection) {
    EXPECT_CALL(*gatt_connection_alias_, IsConnected()).WillOnce(Return(true));
    EXPECT_CALL(*connection, CreateCharacteristicsFinder(_, _))
        .WillOnce(
            DoAll(SaveArg<0>(&characteristics_finder_success_callback_),
                  SaveArg<1>(&characteristics_finder_error_callback_),
                  Return(new MockBluetoothLowEnergyCharacteristicsFinder)));

    connection->Connect();

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_CHARACTERISTICS);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from DISCONNECTED to WAITING_CHARACTERISTICS
  // state, without an existing GATT connection.
  void ConnectWithoutExistingGattConnection(
      MockBluetoothLowEnergyConnection* connection) {
    // Preparing |connection| for a CreateGattConnection call.
    EXPECT_CALL(*gatt_connection_alias_, IsConnected()).WillOnce(Return(false));
    EXPECT_CALL(*device_, CreateGattConnection(_, _))
        .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                        SaveArg<1>(&create_gatt_connection_error_callback_)));

    connection->Connect();

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_GATT_CONNECTION);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);

    // Preparing |connection| to run |create_gatt_connection_success_callback_|.
    EXPECT_FALSE(create_gatt_connection_error_callback_.is_null());
    ASSERT_FALSE(create_gatt_connection_success_callback_.is_null());
    EXPECT_CALL(*connection, CreateCharacteristicsFinder(_, _))
        .WillOnce(DoAll(
            SaveArg<0>(&characteristics_finder_success_callback_),
            SaveArg<1>(&characteristics_finder_error_callback_),
            Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>)));

    create_gatt_connection_success_callback_.Run(make_scoped_ptr(
        new NiceMock<device::MockBluetoothGattConnection>(kBluetoothAddress)));

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_CHARACTERISTICS);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_CHARACTERISTICS to
  // WAITING_NOTIFY_SESSION state.
  void CharacteristicsFound(MockBluetoothLowEnergyConnection* connection) {
    EXPECT_CALL(*from_peripheral_char_, StartNotifySession(_, _))
        .WillOnce(DoAll(SaveArg<0>(&notify_session_success_callback_),
                        SaveArg<1>(&notify_session_error_callback_)));
    EXPECT_FALSE(characteristics_finder_error_callback_.is_null());
    ASSERT_FALSE(characteristics_finder_success_callback_.is_null());

    characteristics_finder_success_callback_.Run(
        {service_uuid_, kServiceID},
        {to_peripheral_char_uuid_, kToPeripheralCharID},
        {from_peripheral_char_uuid_, kFromPeripheralCharID});

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_NOTIFY_SESSION);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_NOTIFY_SESSION to
  // WAITING_RESPONSE_SIGNAL state.
  void NotifySessionStarted(MockBluetoothLowEnergyConnection* connection) {
    EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
        .WillOnce(
            DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                  SaveArg<1>(&write_remote_characteristic_success_callback_),
                  SaveArg<2>(&write_remote_characteristic_error_callback_)));
    EXPECT_FALSE(notify_session_error_callback_.is_null());
    ASSERT_FALSE(notify_session_success_callback_.is_null());

    // Store an alias for the notify session passed |connection|.
    scoped_ptr<device::MockBluetoothGattNotifySession> notify_session(
        new NiceMock<device::MockBluetoothGattNotifySession>(
            kToPeripheralCharID));
    notify_session_alias_ = notify_session.get();
    notify_session_success_callback_.Run(notify_session.Pass());

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_RESPONSE_SIGNAL);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_RESPONSE_SIGNAL to CONNECTED state.
  void ResponseSignalReceived(MockBluetoothLowEnergyConnection* connection) {
    // Written value contains only the
    // BluetoothLowEneryConnection::ControlSignal::kInviteToConnectSignal.
    const std::vector<uint8> kInviteToConnectSignal = ToByteVector(static_cast<
        uint32>(
        BluetoothLowEnergyConnection::ControlSignal::kInviteToConnectSignal));
    EXPECT_EQ(last_value_written_on_to_peripheral_char_,
              kInviteToConnectSignal);

    EXPECT_CALL(*connection, OnDidSendMessage(_, _)).Times(0);
    RunWriteCharacteristicSuccessCallback();

    // Received the
    // BluetoothLowEneryConnection::ControlSignal::kInvitationResponseSignal.
    const std::vector<uint8> kInvitationResponseSignal = ToByteVector(
        static_cast<uint32>(BluetoothLowEnergyConnection::ControlSignal::
                                kInvitationResponseSignal));
    connection->GattCharacteristicValueChanged(
        adapter_.get(), from_peripheral_char_.get(), kInvitationResponseSignal);

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::CONNECTED);
    EXPECT_EQ(connection->status(), Connection::CONNECTED);
  }

  // Transitions |connection| to a DISCONNECTED state regardless of its initial
  // state.
  void Disconnect(MockBluetoothLowEnergyConnection* connection) {
    // A notify session was previously set.
    if (notify_session_alias_)
      EXPECT_CALL(*notify_session_alias_, Stop(_));

    connection->Disconnect();

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
  }

  void InitializeConnection(MockBluetoothLowEnergyConnection* connection) {
    ConnectWithExistingGattConnection(connection);
    CharacteristicsFound(connection);
    NotifySessionStarted(connection);
    ResponseSignalReceived(connection);
  }

  void RunWriteCharacteristicSuccessCallback() {
    EXPECT_FALSE(write_remote_characteristic_error_callback_.is_null());
    ASSERT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_success_callback_.Run();
  }

  std::vector<uint8> CreateSendSignalWithSize(int message_size) {
    std::vector<uint8> value = ToByteVector(static_cast<uint32>(
        BluetoothLowEnergyConnection::ControlSignal::kSendSignal));
    std::vector<uint8> size = ToByteVector(static_cast<uint32>(message_size));
    value.insert(value.end(), size.begin(), size.end());
    return value;
  }

  std::vector<uint8> ToByteVector(uint32 value) {
    std::vector<uint8> bytes(4, 0);
    bytes[0] = static_cast<uint8>(value);
    bytes[1] = static_cast<uint8>(value >> 8);
    bytes[2] = static_cast<uint8>(value >> 16);
    bytes[3] = static_cast<uint8>(value >> 24);
    return bytes;
  }

 protected:
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  RemoteDevice remote_device_;
  device::BluetoothUUID service_uuid_;
  device::BluetoothUUID to_peripheral_char_uuid_;
  device::BluetoothUUID from_peripheral_char_uuid_;
  scoped_ptr<device::MockBluetoothGattConnection> gatt_connection_;
  device::MockBluetoothGattConnection* gatt_connection_alias_;
  scoped_ptr<device::MockBluetoothDevice> device_;
  scoped_ptr<device::MockBluetoothGattService> service_;
  scoped_ptr<device::MockBluetoothGattCharacteristic> to_peripheral_char_;
  scoped_ptr<device::MockBluetoothGattCharacteristic> from_peripheral_char_;
  std::vector<uint8> last_value_written_on_to_peripheral_char_;
  device::MockBluetoothGattNotifySession* notify_session_alias_;

  // Callbacks
  device::BluetoothDevice::GattConnectionCallback
      create_gatt_connection_success_callback_;
  device::BluetoothDevice::ConnectErrorCallback
      create_gatt_connection_error_callback_;

  BluetoothLowEnergyCharacteristicsFinder::SuccessCallback
      characteristics_finder_success_callback_;
  BluetoothLowEnergyCharacteristicsFinder::ErrorCallback
      characteristics_finder_error_callback_;

  device::BluetoothGattCharacteristic::NotifySessionCallback
      notify_session_success_callback_;
  device::BluetoothGattCharacteristic::ErrorCallback
      notify_session_error_callback_;

  base::Closure write_remote_characteristic_success_callback_;
  device::BluetoothGattCharacteristic::ErrorCallback
      write_remote_characteristic_error_callback_;
};

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       CreateAndDestroyWithouthConnectCallDoesntCrash) {
  BluetoothLowEnergyConnection connection(
      remote_device_, adapter_, service_uuid_, to_peripheral_char_uuid_,
      from_peripheral_char_uuid_, gatt_connection_.Pass(), kMaxNumberOfTries);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Disconect_WithoutConnectDoesntCrash) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Success_WithGattConnection) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ResponseSignalReceived(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Success_WithoutGattConnection) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithoutExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ResponseSignalReceived(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Success_Disconnect) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  InitializeConnection(connection.get());
  Disconnect(connection.get());
}

TEST_F(
    ProximityAuthBluetoothLowEnergyConnectionTest,
    Connect_Incomplete_Disconnect_FromWaitingCharacteristicsStateWithoutExistingGattConnection) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithoutExistingGattConnection(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingCharacteristicsState) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingNotifySessionState) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingResponseSignalState) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_CharacteristicsNotFound) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());

  EXPECT_CALL(*from_peripheral_char_, StartNotifySession(_, _)).Times(0);
  EXPECT_FALSE(characteristics_finder_success_callback_.is_null());
  ASSERT_FALSE(characteristics_finder_error_callback_.is_null());

  characteristics_finder_error_callback_.Run(
      {to_peripheral_char_uuid_, kToPeripheralCharID},
      {from_peripheral_char_uuid_, kFromPeripheralCharID});

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_NotifySessionError) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());

  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .Times(0);
  EXPECT_FALSE(notify_session_success_callback_.is_null());
  ASSERT_FALSE(notify_session_error_callback_.is_null());

  notify_session_error_callback_.Run(
      device::BluetoothGattService::GATT_ERROR_UNKNOWN);

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_ErrorSendingInviteToConnectSignal) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithExistingGattConnection(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());

  // |connection| will call WriteRemoteCharacteristics(_,_) to try to send the
  // message |kMaxNumberOfTries| times. There is alredy one EXPECTA_CALL for
  // WriteRemoteCharacteristic(_,_,_) in NotifySessionStated, that's why we use
  // |kMaxNumberOfTries-1| in the EXPECT_CALL statement.
  EXPECT_CALL(*connection, OnDidSendMessage(_, _)).Times(0);
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .Times(kMaxNumberOfTries - 1)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    const std::vector<uint8> kInviteToConnectSignal = ToByteVector(static_cast<
        uint32>(
        BluetoothLowEnergyConnection::ControlSignal::kInviteToConnectSignal));
    EXPECT_EQ(last_value_written_on_to_peripheral_char_,
              kInviteToConnectSignal);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_error_callback_.Run(
        device::BluetoothGattService::GATT_ERROR_UNKNOWN);
  }

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_CharacteristicsNotFound_WithoutExistingGattConnection) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  ConnectWithoutExistingGattConnection(connection.get());

  EXPECT_CALL(*from_peripheral_char_, StartNotifySession(_, _)).Times(0);
  EXPECT_FALSE(characteristics_finder_success_callback_.is_null());
  ASSERT_FALSE(characteristics_finder_error_callback_.is_null());

  characteristics_finder_error_callback_.Run(
      {to_peripheral_char_uuid_, kToPeripheralCharID},
      {from_peripheral_char_uuid_, kFromPeripheralCharID});

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Receive_MessageSmallerThanCharacteristicSize) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  InitializeConnection(connection.get());

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  // Message (bytes) that is going to be received.
  int message_size = 75;
  std::string message(message_size, 'A');

  // Sending the |kSendSignal| + |message_size|.
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(),
      CreateSendSignalWithSize(message_size));

  // Sending the message.
  std::vector<uint8> value;
  value.push_back(0);
  value.insert(value.end(), message.begin(), message.end());
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(), value);

  EXPECT_EQ(received_bytes, message);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Receive_MessageLargerThanCharacteristicSize) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  InitializeConnection(connection.get());

  std::string received_bytes;
  int chunk_size = 100;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  // Message (bytes) that is going to be received.
  int message_size = 150;
  std::string message(message_size, 'A');

  // Sending the |kSendSignal| + |message_size|.
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(),
      CreateSendSignalWithSize(message_size));

  // Sending the first chunk.
  std::vector<uint8> value;
  value.push_back(0);
  value.insert(value.end(), message.begin(), message.begin() + chunk_size);
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(), value);

  // Sending the second chunk.
  value.clear();
  value.push_back(0);
  value.insert(value.end(), message.begin() + chunk_size, message.end());
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(), value);

  EXPECT_EQ(received_bytes, message);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       SendMessage_SmallerThanCharacteristicSize) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  InitializeConnection(connection.get());

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  // Message (bytes) that is going to be sent.
  int message_size = 75;
  std::string message(message_size, 'A');
  message[0] = 'B';
  connection->SendMessage(make_scoped_ptr(new FakeWireMessage(message)));

  // Expecting that |kSendSignal| + |message_size| was written.
  EXPECT_EQ(last_value_written_on_to_peripheral_char_,
            CreateSendSignalWithSize(message_size));

  // Expecting a second call of WriteRemoteCharacteristic, after success
  // callback is called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();

  // Expecting that the message was written.
  std::vector<uint8> expected_value(message.begin(), message.end());
  std::vector<uint8> written_value(
      last_value_written_on_to_peripheral_char_.begin() + 1,
      last_value_written_on_to_peripheral_char_.end());
  EXPECT_EQ(expected_value, written_value);
  EXPECT_EQ(expected_value.size(), written_value.size());

  EXPECT_CALL(*connection, OnDidSendMessage(_, _));
  RunWriteCharacteristicSuccessCallback();
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       SendMessage_LagerThanCharacteristicSize) {
  scoped_ptr<MockBluetoothLowEnergyConnection> connection(CreateConnection());
  InitializeConnection(connection.get());

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  // Message (bytes) that is going to be sent.
  int message_size = 150;
  std::string message(message_size, 'A');
  message[0] = 'B';
  connection->SendMessage(make_scoped_ptr(new FakeWireMessage(message)));

  // Expecting that |kSendSignal| + |message_size| was written.
  EXPECT_EQ(last_value_written_on_to_peripheral_char_,
            CreateSendSignalWithSize(message_size));

  // Expecting a second call of WriteRemoteCharacteristic, after success
  // callback is called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();
  std::vector<uint8> bytes_received(
      last_value_written_on_to_peripheral_char_.begin() + 1,
      last_value_written_on_to_peripheral_char_.end());

  // Expecting a third call of WriteRemoteCharacteristic, after success callback
  // is called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();
  bytes_received.insert(bytes_received.end(),
                        last_value_written_on_to_peripheral_char_.begin() + 1,
                        last_value_written_on_to_peripheral_char_.end());

  // Expecting that the message was written.
  std::vector<uint8> expected_value(message.begin(), message.end());
  EXPECT_EQ(expected_value.size(), bytes_received.size());
  EXPECT_EQ(expected_value, bytes_received);

  EXPECT_CALL(*connection, OnDidSendMessage(_, _));
  RunWriteCharacteristicSuccessCallback();
}

}  // namespace proximity_auth
