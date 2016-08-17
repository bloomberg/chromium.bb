// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/ble/bluetooth_low_energy_connection.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/proximity_auth/ble/bluetooth_low_energy_characteristics_finder.h"
#include "components/proximity_auth/bluetooth_throttler.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/proximity_auth_test_util.h"
#include "components/proximity_auth/remote_device.h"
#include "components/proximity_auth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
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

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kToPeripheralCharUUID[] = "977c6674-1239-4e72-993b-502369b8bb5a";
const char kFromPeripheralCharUUID[] = "f4b904a2-a030-43b3-98a8-221c536c03cb";

const char kServiceID[] = "service id";
const char kToPeripheralCharID[] = "to peripheral char id";
const char kFromPeripheralCharID[] = "from peripheral char id";

const device::BluetoothRemoteGattCharacteristic::Properties
    kCharacteristicProperties =
        device::BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        device::BluetoothRemoteGattCharacteristic::
            PROPERTY_WRITE_WITHOUT_RESPONSE |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const int kMaxNumberOfTries = 3;

class MockBluetoothThrottler : public BluetoothThrottler {
 public:
  MockBluetoothThrottler() {}
  ~MockBluetoothThrottler() override {}

  MOCK_CONST_METHOD0(GetDelay, base::TimeDelta());
  MOCK_METHOD1(OnConnection, void(Connection* connection));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothThrottler);
};

class MockBluetoothLowEnergyCharacteristicsFinder
    : public BluetoothLowEnergyCharacteristicsFinder {
 public:
  MockBluetoothLowEnergyCharacteristicsFinder() {}
  ~MockBluetoothLowEnergyCharacteristicsFinder() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothLowEnergyCharacteristicsFinder);
};

class MockBluetoothLowEnergyConnection : public BluetoothLowEnergyConnection {
 public:
  MockBluetoothLowEnergyConnection(
      const RemoteDevice& remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      BluetoothThrottler* bluetooth_throttler,
      int max_number_of_write_attempts)
      : BluetoothLowEnergyConnection(remote_device,
                                     adapter,
                                     remote_service_uuid,
                                     bluetooth_throttler,
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
  using BluetoothLowEnergyConnection::SetTaskRunnerForTesting;

  // Exposing inherited protected fields for testing.
  using BluetoothLowEnergyConnection::status;
  using BluetoothLowEnergyConnection::sub_status;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothLowEnergyConnection);
};

}  // namespace

class ProximityAuthBluetoothLowEnergyConnectionTest : public testing::Test {
 public:
  ProximityAuthBluetoothLowEnergyConnectionTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        remote_device_(CreateLERemoteDeviceForTest()),
        service_uuid_(device::BluetoothUUID(kServiceUUID)),
        to_peripheral_char_uuid_(device::BluetoothUUID(kToPeripheralCharUUID)),
        from_peripheral_char_uuid_(
            device::BluetoothUUID(kFromPeripheralCharUUID)),
        notify_session_alias_(NULL),
        bluetooth_throttler_(new NiceMock<MockBluetoothThrottler>),
        task_runner_(new base::TestSimpleTaskRunner) {}

  void SetUp() override {
    device_ = base::WrapUnique(new NiceMock<device::MockBluetoothDevice>(
        adapter_.get(), 0, kTestRemoteDeviceName,
        kTestRemoteDeviceBluetoothAddress, false, false));

    service_ = base::WrapUnique(new NiceMock<device::MockBluetoothGattService>(
        device_.get(), kServiceID, service_uuid_, true, false));
    to_peripheral_char_ =
        base::WrapUnique(new NiceMock<device::MockBluetoothGattCharacteristic>(
            service_.get(), kToPeripheralCharID, to_peripheral_char_uuid_,
            false, kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE));

    from_peripheral_char_ =
        base::WrapUnique(new NiceMock<device::MockBluetoothGattCharacteristic>(
            service_.get(), kFromPeripheralCharID, from_peripheral_char_uuid_,
            false, kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE));

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::vector<const device::BluetoothDevice*> devices;
    devices.push_back(device_.get());
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
    ON_CALL(*adapter_, GetDevice(kTestRemoteDeviceBluetoothAddress))
        .WillByDefault(Return(device_.get()));
    ON_CALL(*device_, GetGattService(kServiceID))
        .WillByDefault(Return(service_.get()));
    ON_CALL(*service_, GetCharacteristic(kFromPeripheralCharID))
        .WillByDefault(Return(from_peripheral_char_.get()));
    ON_CALL(*service_, GetCharacteristic(kToPeripheralCharID))
        .WillByDefault(Return(to_peripheral_char_.get()));
  }

  // Creates a BluetoothLowEnergyConnection and verifies it's in DISCONNECTED
  // state.
  std::unique_ptr<MockBluetoothLowEnergyConnection> CreateConnection() {
    EXPECT_CALL(*adapter_, AddObserver(_));
    EXPECT_CALL(*adapter_, RemoveObserver(_));

    std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
        new MockBluetoothLowEnergyConnection(
            remote_device_, adapter_, service_uuid_, bluetooth_throttler_.get(),
            kMaxNumberOfTries));

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::DISCONNECTED);

    connection->SetTaskRunnerForTesting(task_runner_);

    return connection;
  }

  // Transitions |connection| from DISCONNECTED to WAITING_CHARACTERISTICS
  // state, without an existing GATT connection.
  void ConnectGatt(MockBluetoothLowEnergyConnection* connection) {
    // Preparing |connection| for a CreateGattConnection call.
    EXPECT_CALL(*device_, CreateGattConnection(_, _))
        .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                        SaveArg<1>(&create_gatt_connection_error_callback_)));

    // No throttling by default
    EXPECT_CALL(*bluetooth_throttler_, GetDelay())
        .WillOnce(Return(base::TimeDelta()));

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

    create_gatt_connection_success_callback_.Run(
        base::WrapUnique(new NiceMock<device::MockBluetoothGattConnection>(
            adapter_, kTestRemoteDeviceBluetoothAddress)));

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
    std::unique_ptr<device::MockBluetoothGattNotifySession> notify_session(
        new NiceMock<device::MockBluetoothGattNotifySession>(
            kToPeripheralCharID));
    notify_session_alias_ = notify_session.get();

    notify_session_success_callback_.Run(std::move(notify_session));
    task_runner_->RunUntilIdle();

    EXPECT_EQ(connection->sub_status(),
              BluetoothLowEnergyConnection::SubStatus::WAITING_RESPONSE_SIGNAL);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_RESPONSE_SIGNAL to CONNECTED state.
  void ResponseSignalReceived(MockBluetoothLowEnergyConnection* connection) {
    // Written value contains only the
    // BluetoothLowEneryConnection::ControlSignal::kInviteToConnectSignal.
    const std::vector<uint8_t> kInviteToConnectSignal = ToByteVector(
        static_cast<uint32_t>(BluetoothLowEnergyConnection::ControlSignal::
                                  kInviteToConnectSignal));
    EXPECT_EQ(last_value_written_on_to_peripheral_char_,
              kInviteToConnectSignal);

    EXPECT_CALL(*connection, OnDidSendMessage(_, _)).Times(0);
    RunWriteCharacteristicSuccessCallback();

    // Received the
    // BluetoothLowEneryConnection::ControlSignal::kInvitationResponseSignal.
    const std::vector<uint8_t> kInvitationResponseSignal = ToByteVector(
        static_cast<uint32_t>(BluetoothLowEnergyConnection::ControlSignal::
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
    ConnectGatt(connection);
    CharacteristicsFound(connection);
    NotifySessionStarted(connection);
    ResponseSignalReceived(connection);
  }

  void RunWriteCharacteristicSuccessCallback() {
    EXPECT_FALSE(write_remote_characteristic_error_callback_.is_null());
    ASSERT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_success_callback_.Run();
  }

  std::vector<uint8_t> CreateSendSignalWithSize(int message_size) {
    std::vector<uint8_t> value = ToByteVector(static_cast<uint32_t>(
        BluetoothLowEnergyConnection::ControlSignal::kSendSignal));
    std::vector<uint8_t> size =
        ToByteVector(static_cast<uint32_t>(message_size));
    value.insert(value.end(), size.begin(), size.end());
    return value;
  }

  std::vector<uint8_t> CreateFirstCharacteristicValue(
      const std::string& message,
      int size) {
    std::vector<uint8_t> value(CreateSendSignalWithSize(size));
    std::vector<uint8_t> bytes(message.begin(), message.end());
    value.insert(value.end(), bytes.begin(), bytes.end());
    return value;
  }

  std::vector<uint8_t> ToByteVector(uint32_t value) {
    std::vector<uint8_t> bytes(4, 0);
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
    bytes[2] = static_cast<uint8_t>(value >> 16);
    bytes[3] = static_cast<uint8_t>(value >> 24);
    return bytes;
  }

 protected:
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  RemoteDevice remote_device_;
  device::BluetoothUUID service_uuid_;
  device::BluetoothUUID to_peripheral_char_uuid_;
  device::BluetoothUUID from_peripheral_char_uuid_;
  std::unique_ptr<device::MockBluetoothDevice> device_;
  std::unique_ptr<device::MockBluetoothGattService> service_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> to_peripheral_char_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic>
      from_peripheral_char_;
  std::vector<uint8_t> last_value_written_on_to_peripheral_char_;
  device::MockBluetoothGattNotifySession* notify_session_alias_;
  std::unique_ptr<MockBluetoothThrottler> bluetooth_throttler_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::MessageLoop message_loop_;

  // Callbacks
  device::BluetoothDevice::GattConnectionCallback
      create_gatt_connection_success_callback_;
  device::BluetoothDevice::ConnectErrorCallback
      create_gatt_connection_error_callback_;

  BluetoothLowEnergyCharacteristicsFinder::SuccessCallback
      characteristics_finder_success_callback_;
  BluetoothLowEnergyCharacteristicsFinder::ErrorCallback
      characteristics_finder_error_callback_;

  device::BluetoothRemoteGattCharacteristic::NotifySessionCallback
      notify_session_success_callback_;
  device::BluetoothRemoteGattCharacteristic::ErrorCallback
      notify_session_error_callback_;

  base::Closure write_remote_characteristic_success_callback_;
  device::BluetoothRemoteGattCharacteristic::ErrorCallback
      write_remote_characteristic_error_callback_;
};

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       CreateAndDestroyWithouthConnectCallDoesntCrash) {
  BluetoothLowEnergyConnection connection(
      remote_device_, adapter_, service_uuid_, bluetooth_throttler_.get(),
      kMaxNumberOfTries);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Disconect_WithoutConnectDoesntCrash) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest, Connect_Success) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ResponseSignalReceived(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Success_Disconnect) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingCharacteristicsState) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingNotifySessionState) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Incomplete_Disconnect_FromWaitingResponseSignalState) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  Disconnect(connection.get());
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_CharacteristicsNotFound) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());

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
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());

  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .Times(0);
  EXPECT_FALSE(notify_session_success_callback_.is_null());
  ASSERT_FALSE(notify_session_error_callback_.is_null());

  notify_session_error_callback_.Run(
      device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_Fails_ErrorSendingInviteToConnectSignal) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
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
    const std::vector<uint8_t> kInviteToConnectSignal = ToByteVector(
        static_cast<uint32_t>(BluetoothLowEnergyConnection::ControlSignal::
                                  kInviteToConnectSignal));
    EXPECT_EQ(last_value_written_on_to_peripheral_char_,
              kInviteToConnectSignal);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_error_callback_.Run(
        device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  }

  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Receive_MessageSmallerThanCharacteristicSize) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get());

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  // Message (bytes) that is going to be received.
  std::string message(100, 'A');

  // Sending the |kSendSignal| + |message_size| + |message|.
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(),
      CreateFirstCharacteristicValue(message, message.size()));

  EXPECT_EQ(received_bytes, message);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Receive_MessageLargerThanCharacteristicSize) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get());

  std::string received_bytes;
  int chunk_size = 500;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  // Message (bytes) that is going to be received.
  int message_size = 600;
  std::string message(message_size, 'A');

  // Sending the |kSendSignal| + |message_size| + |message| (truncated at
  // |chunk_size|).
  int first_write_payload_size =
      chunk_size - CreateSendSignalWithSize(message_size).size();
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(),
      CreateFirstCharacteristicValue(
          message.substr(0, first_write_payload_size), message.size()));

  // Sending the remaining bytes.
  std::vector<uint8_t> value;
  value.push_back(0);
  value.insert(value.end(), message.begin() + first_write_payload_size,
               message.end());
  connection->GattCharacteristicValueChanged(
      adapter_.get(), from_peripheral_char_.get(), value);

  EXPECT_EQ(received_bytes, message);
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       SendMessage_SmallerThanCharacteristicSize) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get());

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  // Message (bytes) that is going to be sent.
  int message_size = 100;
  std::string message(message_size, 'A');
  message[0] = 'B';
  connection->SendMessage(base::WrapUnique(new FakeWireMessage(message)));

  // Expecting that |kSendSignal| + |message_size| + |message| was written.
  EXPECT_EQ(last_value_written_on_to_peripheral_char_,
            CreateFirstCharacteristicValue(message, message.size()));
  EXPECT_CALL(*connection, OnDidSendMessage(_, _));

  RunWriteCharacteristicSuccessCallback();
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       SendMessage_LagerThanCharacteristicSize) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get());

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*to_peripheral_char_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_to_peripheral_char_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  // Message (bytes) that is going to be sent.
  int message_size = 600;
  std::string message(message_size, 'A');
  message[0] = 'B';
  connection->SendMessage(base::WrapUnique(new FakeWireMessage(message)));

  // Expecting that |kSendSignal| + |message_size| was written in the first 8
  // bytes.
  std::vector<uint8_t> prefix(
      last_value_written_on_to_peripheral_char_.begin(),
      last_value_written_on_to_peripheral_char_.begin() + 8);
  EXPECT_EQ(prefix, CreateSendSignalWithSize(message_size));
  std::vector<uint8_t> bytes_received(
      last_value_written_on_to_peripheral_char_.begin() + 8,
      last_value_written_on_to_peripheral_char_.end());

  // Expecting a second call of WriteRemoteCharacteristic, after success
  // callback is called.
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
  std::vector<uint8_t> expected_value(message.begin(), message.end());
  EXPECT_EQ(expected_value.size(), bytes_received.size());
  EXPECT_EQ(expected_value, bytes_received);

  EXPECT_CALL(*connection, OnDidSendMessage(_, _));
  RunWriteCharacteristicSuccessCallback();
}

TEST_F(ProximityAuthBluetoothLowEnergyConnectionTest,
       Connect_AfterADelayWhenThrottled) {
  std::unique_ptr<MockBluetoothLowEnergyConnection> connection(
      CreateConnection());

  EXPECT_CALL(*bluetooth_throttler_, GetDelay())
      .WillOnce(Return(base::TimeDelta(base::TimeDelta::FromSeconds(1))));
  EXPECT_CALL(*device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                      SaveArg<1>(&create_gatt_connection_error_callback_)));

  // No GATT connection should be created before the delay.
  connection->Connect();
  EXPECT_EQ(connection->sub_status(),
            BluetoothLowEnergyConnection::SubStatus::WAITING_GATT_CONNECTION);
  EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  EXPECT_TRUE(create_gatt_connection_error_callback_.is_null());
  EXPECT_TRUE(create_gatt_connection_success_callback_.is_null());

  // A GATT connection should be created after the delay.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(create_gatt_connection_error_callback_.is_null());
  ASSERT_FALSE(create_gatt_connection_success_callback_.is_null());

  // Preparing |connection| to run |create_gatt_connection_success_callback_|.
  EXPECT_CALL(*connection, CreateCharacteristicsFinder(_, _))
      .WillOnce(DoAll(
          SaveArg<0>(&characteristics_finder_success_callback_),
          SaveArg<1>(&characteristics_finder_error_callback_),
          Return(new NiceMock<MockBluetoothLowEnergyCharacteristicsFinder>)));

  create_gatt_connection_success_callback_.Run(
      base::WrapUnique(new NiceMock<device::MockBluetoothGattConnection>(
          adapter_, kTestRemoteDeviceBluetoothAddress)));

  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ResponseSignalReceived(connection.get());
}

}  // namespace proximity_auth
