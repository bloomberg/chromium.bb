// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/ble/bluetooth_low_energy_weave_client_connection.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "components/cryptauth/bluetooth_throttler.h"
#include "components/cryptauth/connection_finder.h"
#include "components/cryptauth/connection_observer.h"
#include "components/cryptauth/cryptauth_test_util.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/wire_message.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
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

namespace cryptauth {
namespace {

const std::string kTestFeature = "testFeature";

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

class MockConnectionObserver : public ConnectionObserver {
 public:
  MockConnectionObserver() : num_send_completed_(0) {}

  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override {}

  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override {}

  void OnSendCompleted(const Connection& conenction,
                       const WireMessage& message,
                       bool success) override {
    last_deserialized_message_ = message.payload();
    last_send_success_ = success;
    num_send_completed_++;
  }

  std::string GetLastDeserializedMessage() {
    return last_deserialized_message_;
  }

  bool GetLastSendSuccess() { return last_send_success_; }

  int GetNumSendCompleted() { return num_send_completed_; }

 private:
  std::string last_deserialized_message_;
  bool last_send_success_;
  int num_send_completed_;
};

}  // namespace

namespace weave {
namespace {

typedef BluetoothLowEnergyWeaveClientConnection::SubStatus SubStatus;
typedef BluetoothLowEnergyWeavePacketReceiver::State ReceiverState;
typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverError ReceiverError;
typedef BluetoothLowEnergyWeavePacketReceiver::ReceiverType ReceiverType;

const char kServiceUUID[] = "DEADBEEF-CAFE-FEED-FOOD-D15EA5EBEEEF";
const char kTXCharacteristicUUID[] = "977c6674-1239-4e72-993b-502369b8bb5a";
const char kRXCharacteristicUUID[] = "f4b904a2-a030-43b3-98a8-221c536c03cb";

const char kServiceID[] = "service id";
const char kTXCharacteristicID[] = "TX characteristic id";
const char kRXCharacteristicID[] = "RX characteristic id";

const device::BluetoothRemoteGattCharacteristic::Properties
    kCharacteristicProperties =
        device::BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        device::BluetoothRemoteGattCharacteristic::
            PROPERTY_WRITE_WITHOUT_RESPONSE |
        device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;

const int kMaxNumberOfTries = 3;
const uint16_t kLargeMaxPacketSize = 30;

const uint8_t kDataHeader = 0;
const uint8_t kConnectionRequestHeader = 1;
const uint8_t kSmallConnectionResponseHeader = 2;
const uint8_t kLargeConnectionResponseHeader = 3;
const uint8_t kConnectionCloseHeader = 4;
const uint8_t kErroneousHeader = 5;

const std::string kSmallMessage = "bb";
const std::string kLargeMessage = "aaabbb";
const std::string kLargeMessage0 = "aaa";
const std::string kLargeMessage1 = "bbb";

const Packet kConnectionRequest{kConnectionRequestHeader};
const Packet kSmallConnectionResponse{kSmallConnectionResponseHeader};
const Packet kLargeConnectionResponse{kLargeConnectionResponseHeader};
const Packet kConnectionCloseSuccess{kConnectionCloseHeader,
                                     ReasonForClose::CLOSE_WITHOUT_ERROR};
const Packet kConnectionCloseUnknownError{kConnectionCloseHeader,
                                          ReasonForClose::UNKNOWN_ERROR};
const Packet kConnectionCloseApplicationError{
    kConnectionCloseHeader, ReasonForClose::APPLICATION_ERROR};

const Packet kSmallPackets0 = Packet{kDataHeader, 'b', 'b'};
const Packet kLargePackets0 = Packet{kDataHeader, 'a', 'a', 'a'};
const Packet kLargePackets1 = Packet{kDataHeader, 'b', 'b', 'b'};
const Packet kErroneousPacket = Packet{kErroneousHeader};

const std::vector<Packet> kSmallPackets{kSmallPackets0};
const std::vector<Packet> kLargePackets{kLargePackets0, kLargePackets1};

class MockBluetoothLowEnergyWeavePacketGenerator
    : public BluetoothLowEnergyWeavePacketGenerator {
 public:
  MockBluetoothLowEnergyWeavePacketGenerator()
      : max_packet_size_(kDefaultMaxPacketSize) {}

  Packet CreateConnectionRequest() override { return kConnectionRequest; }

  Packet CreateConnectionResponse() override {
    NOTIMPLEMENTED();
    return Packet();
  }

  Packet CreateConnectionClose(ReasonForClose reason_for_close) override {
    return Packet{kConnectionCloseHeader,
                  static_cast<uint8_t>(reason_for_close)};
  }

  void SetMaxPacketSize(uint16_t size) override { max_packet_size_ = size; }

  std::vector<Packet> EncodeDataMessage(std::string message) override {
    if (message == (kTestFeature + "," + kSmallMessage)
        && max_packet_size_ == kDefaultMaxPacketSize) {
      return kSmallPackets;
    } else if (message == (kTestFeature + "," + kLargeMessage)
          && max_packet_size_ == kLargeMaxPacketSize) {
      return kLargePackets;
    } else {
      NOTREACHED();
      return std::vector<Packet>();
    }
  }

  uint16_t GetMaxPacketSize() { return max_packet_size_; }

 private:
  uint16_t max_packet_size_;
};

class MockBluetoothLowEnergyWeavePacketReceiver
    : public BluetoothLowEnergyWeavePacketReceiver {
 public:
  MockBluetoothLowEnergyWeavePacketReceiver()
      : BluetoothLowEnergyWeavePacketReceiver(ReceiverType::CLIENT),
        state_(State::CONNECTING),
        max_packet_size_(kDefaultMaxPacketSize),
        reason_for_close_(ReasonForClose::CLOSE_WITHOUT_ERROR),
        reason_to_close_(ReasonForClose::CLOSE_WITHOUT_ERROR) {}

  ReceiverState GetState() override { return state_; }

  uint16_t GetMaxPacketSize() override { return max_packet_size_; }

  ReasonForClose GetReasonForClose() override { return reason_for_close_; }

  ReasonForClose GetReasonToClose() override { return reason_to_close_; }

  std::string GetDataMessage() override {
    if (max_packet_size_ == kDefaultMaxPacketSize) {
      return kSmallMessage;
    } else {
      return kLargeMessage;
    }
  }

  ReceiverError GetReceiverError() override {
    return ReceiverError::NO_ERROR_DETECTED;
  }

  ReceiverState ReceivePacket(const Packet& packet) override {
    switch (packet[0]) {
      case kSmallConnectionResponseHeader:
        max_packet_size_ = kDefaultMaxPacketSize;
        state_ = ReceiverState::WAITING;
        break;
      case kLargeConnectionResponseHeader:
        max_packet_size_ = kLargeMaxPacketSize;
        state_ = ReceiverState::WAITING;
        break;
      case kConnectionCloseHeader:
        state_ = ReceiverState::CONNECTION_CLOSED;
        reason_for_close_ = static_cast<ReasonForClose>(packet[1]);
        break;
      case kDataHeader:
        if (packet == kSmallPackets0 || packet == kLargePackets1) {
          state_ = ReceiverState::DATA_READY;
        } else {
          state_ = ReceiverState::RECEIVING_DATA;
        }
        break;
      default:
        reason_to_close_ = ReasonForClose::APPLICATION_ERROR;
        state_ = ReceiverState::ERROR_DETECTED;
    }
    return state_;
  }

 private:
  ReceiverState state_;
  uint16_t max_packet_size_;
  ReasonForClose reason_for_close_;
  ReasonForClose reason_to_close_;
};

class MockBluetoothLowEnergyWeavePacketGeneratorFactory
    : public BluetoothLowEnergyWeavePacketGenerator::Factory {
 public:
  // most_recent_instance_ will be obsolete after the connection class
  // destructs. Do not use if that's the case.
  MockBluetoothLowEnergyWeavePacketGenerator* GetMostRecentInstance() {
    return most_recent_instance_;
  }

 private:
  std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator> BuildInstance()
      override {
    most_recent_instance_ = new MockBluetoothLowEnergyWeavePacketGenerator();
    return std::unique_ptr<BluetoothLowEnergyWeavePacketGenerator>(
        most_recent_instance_);
  }

  MockBluetoothLowEnergyWeavePacketGenerator* most_recent_instance_;
};

class MockBluetoothLowEnergyWeavePacketReceiverFactory
    : public BluetoothLowEnergyWeavePacketReceiver::Factory {
 public:
  // most_recent_instance_ will be obsolete after the connection class
  // destructs. Do not use if that's the case.
  MockBluetoothLowEnergyWeavePacketReceiver* GetMostRecentInstance() {
    return most_recent_instance_;
  }

 private:
  std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver> BuildInstance(
      ReceiverType receiver_type) override {
    most_recent_instance_ = new MockBluetoothLowEnergyWeavePacketReceiver();
    return std::unique_ptr<BluetoothLowEnergyWeavePacketReceiver>(
        most_recent_instance_);
  }

  MockBluetoothLowEnergyWeavePacketReceiver* most_recent_instance_;
};

class TestBluetoothLowEnergyWeaveClientConnection
    : public BluetoothLowEnergyWeaveClientConnection {
 public:
  TestBluetoothLowEnergyWeaveClientConnection(
      const RemoteDevice& remote_device,
      const std::string& device_address,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      BluetoothThrottler* bluetooth_throttler)
      : BluetoothLowEnergyWeaveClientConnection(remote_device,
                                                device_address,
                                                adapter,
                                                remote_service_uuid,
                                                bluetooth_throttler) {}

  ~TestBluetoothLowEnergyWeaveClientConnection() override {}

  MOCK_METHOD2(
      CreateCharacteristicsFinder,
      BluetoothLowEnergyCharacteristicsFinder*(
          const BluetoothLowEnergyCharacteristicsFinder::SuccessCallback&
              success,
          const BluetoothLowEnergyCharacteristicsFinder::ErrorCallback& error));

  MOCK_METHOD1(OnBytesReceived, void(const std::string& bytes));

  // Exposing inherited protected methods for testing.
  using BluetoothLowEnergyWeaveClientConnection::GattCharacteristicValueChanged;
  using BluetoothLowEnergyWeaveClientConnection::SetTaskRunnerForTesting;
  using BluetoothLowEnergyWeaveClientConnection::DestroyConnection;

  // Exposing inherited protected fields for testing.
  using BluetoothLowEnergyWeaveClientConnection::status;
  using BluetoothLowEnergyWeaveClientConnection::sub_status;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBluetoothLowEnergyWeaveClientConnection);
};

}  // namespace

class CryptAuthBluetoothLowEnergyWeaveClientConnectionTest
    : public testing::Test {
 public:
  CryptAuthBluetoothLowEnergyWeaveClientConnectionTest()
      : adapter_(new NiceMock<device::MockBluetoothAdapter>),
        remote_device_(CreateLERemoteDeviceForTest()),
        service_uuid_(device::BluetoothUUID(kServiceUUID)),
        tx_characteristic_uuid_(device::BluetoothUUID(kTXCharacteristicUUID)),
        rx_characteristic_uuid_(device::BluetoothUUID(kRXCharacteristicUUID)),
        notify_session_alias_(NULL),
        bluetooth_throttler_(new NiceMock<MockBluetoothThrottler>),
        task_runner_(new base::TestSimpleTaskRunner),
        generator_factory_(
            new MockBluetoothLowEnergyWeavePacketGeneratorFactory()),
        receiver_factory_(
            new MockBluetoothLowEnergyWeavePacketReceiverFactory()) {
    BluetoothLowEnergyWeavePacketGenerator::Factory::SetInstanceForTesting(
        generator_factory_);
    BluetoothLowEnergyWeavePacketReceiver::Factory::SetInstanceForTesting(
        receiver_factory_);
  }

  ~CryptAuthBluetoothLowEnergyWeaveClientConnectionTest() override {
    BluetoothLowEnergyWeavePacketGenerator::Factory::SetInstanceForTesting(
        nullptr);
    BluetoothLowEnergyWeavePacketReceiver::Factory::SetInstanceForTesting(
        nullptr);
  }

  void SetUp() override {
    mock_bluetooth_device_ =
        base::MakeUnique<NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), 0, kTestRemoteDeviceName,
            kTestRemoteDeviceBluetoothAddress, false, false);

    service_ = base::MakeUnique<NiceMock<device::MockBluetoothGattService>>(
        mock_bluetooth_device_.get(), kServiceID, service_uuid_, true, false);
    tx_characteristic_ =
        base::MakeUnique<NiceMock<device::MockBluetoothGattCharacteristic>>(
            service_.get(), kTXCharacteristicID, tx_characteristic_uuid_, false,
            kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE);

    rx_characteristic_ =
        base::MakeUnique<NiceMock<device::MockBluetoothGattCharacteristic>>(
            service_.get(), kRXCharacteristicID, rx_characteristic_uuid_, false,
            kCharacteristicProperties,
            device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE);

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::vector<const device::BluetoothDevice*> devices;
    devices.push_back(mock_bluetooth_device_.get());
    ON_CALL(*adapter_, GetDevices()).WillByDefault(Return(devices));
    ON_CALL(*adapter_, GetDevice(kTestRemoteDeviceBluetoothAddress))
        .WillByDefault(Return(mock_bluetooth_device_.get()));
    ON_CALL(*mock_bluetooth_device_, GetGattService(kServiceID))
        .WillByDefault(Return(service_.get()));
    ON_CALL(*service_, GetCharacteristic(kRXCharacteristicID))
        .WillByDefault(Return(rx_characteristic_.get()));
    ON_CALL(*service_, GetCharacteristic(kTXCharacteristicID))
        .WillByDefault(Return(tx_characteristic_.get()));
  }

  // Creates a BluetoothLowEnergyWeaveClientConnection and verifies it's in
  // DISCONNECTED state.
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection>
  CreateConnection() {
    EXPECT_CALL(*adapter_, AddObserver(_));
    EXPECT_CALL(*adapter_, RemoveObserver(_));

    std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
        new TestBluetoothLowEnergyWeaveClientConnection(
            remote_device_, kTestRemoteDeviceBluetoothAddress, adapter_,
            service_uuid_, bluetooth_throttler_.get()));

    EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::DISCONNECTED);

    // Add the mock observer to observe on OnDidMessageSend.
    connection->AddObserver(&connection_observer_);

    connection->SetTaskRunnerForTesting(task_runner_);

    return connection;
  }

  // Transitions |connection| from DISCONNECTED to WAITING_CHARACTERISTICS
  // state, without an existing GATT connection.
  void ConnectGatt(TestBluetoothLowEnergyWeaveClientConnection* connection) {
    // Preparing |connection| for a CreateGattConnection call.
    EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
        .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                        SaveArg<1>(&create_gatt_connection_error_callback_)));

    // No throttling by default
    EXPECT_CALL(*bluetooth_throttler_, GetDelay())
        .WillOnce(Return(base::TimeDelta()));

    connection->Connect();

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_GATT_CONNECTION);
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
        base::MakeUnique<NiceMock<device::MockBluetoothGattConnection>>(
            adapter_, kTestRemoteDeviceBluetoothAddress));

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CHARACTERISTICS);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_CHARACTERISTICS to
  // WAITING_NOTIFY_SESSION state.
  void CharacteristicsFound(
      TestBluetoothLowEnergyWeaveClientConnection* connection) {
    EXPECT_CALL(*rx_characteristic_, StartNotifySession(_, _))
        .WillOnce(DoAll(SaveArg<0>(&notify_session_success_callback_),
                        SaveArg<1>(&notify_session_error_callback_)));
    EXPECT_FALSE(characteristics_finder_error_callback_.is_null());
    ASSERT_FALSE(characteristics_finder_success_callback_.is_null());

    characteristics_finder_success_callback_.Run(
        {service_uuid_, kServiceID},
        {tx_characteristic_uuid_, kTXCharacteristicID},
        {rx_characteristic_uuid_, kRXCharacteristicID});

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_NOTIFY_SESSION);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_NOTIFY_SESSION to
  // WAITING_CONNECTION_RESPONSE state.
  void NotifySessionStarted(
      TestBluetoothLowEnergyWeaveClientConnection* connection) {
    EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
        .WillOnce(
            DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                  SaveArg<1>(&write_remote_characteristic_success_callback_),
                  SaveArg<2>(&write_remote_characteristic_error_callback_)));
    EXPECT_FALSE(notify_session_error_callback_.is_null());
    ASSERT_FALSE(notify_session_success_callback_.is_null());

    // Store an alias for the notify session passed |connection|.
    std::unique_ptr<device::MockBluetoothGattNotifySession> notify_session(
        new NiceMock<device::MockBluetoothGattNotifySession>(
            tx_characteristic_->GetWeakPtr()));
    notify_session_alias_ = notify_session.get();

    notify_session_success_callback_.Run(std::move(notify_session));
    task_runner_->RunUntilIdle();

    // Written value contains only the mock Connection Request.
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);

    EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_CONNECTION_RESPONSE);
    EXPECT_EQ(connection->status(), Connection::IN_PROGRESS);
  }

  // Transitions |connection| from WAITING_CONNECTION_RESPONSE to CONNECTED.
  void ConnectionResponseReceived(
      TestBluetoothLowEnergyWeaveClientConnection* connection,
      uint16_t selected_packet_size) {
    // Written value contains only the mock Connection Request.
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);

    // OnDidSendMessage is not called.
    EXPECT_EQ(0, connection_observer_.GetNumSendCompleted());

    RunWriteCharacteristicSuccessCallback();

    // Received Connection Response.
    if (selected_packet_size == kDefaultMaxPacketSize) {
      connection->GattCharacteristicValueChanged(
          adapter_.get(), rx_characteristic_.get(), kSmallConnectionResponse);
      EXPECT_EQ(receiver_factory_->GetMostRecentInstance()->GetMaxPacketSize(),
                kDefaultMaxPacketSize);
      EXPECT_EQ(generator_factory_->GetMostRecentInstance()->GetMaxPacketSize(),
                kDefaultMaxPacketSize);
    } else if (selected_packet_size == kLargeMaxPacketSize) {
      connection->GattCharacteristicValueChanged(
          adapter_.get(), rx_characteristic_.get(), kLargeConnectionResponse);
      EXPECT_EQ(receiver_factory_->GetMostRecentInstance()->GetMaxPacketSize(),
                kLargeMaxPacketSize);
      EXPECT_EQ(generator_factory_->GetMostRecentInstance()->GetMaxPacketSize(),
                kLargeMaxPacketSize);
    } else {
      NOTREACHED();
    }

    EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED);
    EXPECT_EQ(connection->status(), Connection::CONNECTED);
  }

  // Transitions |connection| to a DISCONNECTED state regardless of its initial
  // state.
  void Disconnect(TestBluetoothLowEnergyWeaveClientConnection* connection) {
    // A notify session was previously set.
    if (notify_session_alias_)
      EXPECT_CALL(*notify_session_alias_, Stop(_));

    if (connection->sub_status() == SubStatus::CONNECTED) {
      EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
          .WillOnce(
              DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                    SaveArg<1>(&write_remote_characteristic_success_callback_),
                    SaveArg<2>(&write_remote_characteristic_error_callback_)));
    }

    connection->Disconnect();

    if (connection->sub_status() == SubStatus::CONNECTED) {
      connection->DestroyConnection();
      EXPECT_EQ(last_value_written_on_tx_characteristic_,
                kConnectionCloseSuccess);
      RunWriteCharacteristicSuccessCallback();
    }

    EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
    EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
  }

  void InitializeConnection(
      TestBluetoothLowEnergyWeaveClientConnection* connection,
      uint32_t selected_packet_size) {
    ConnectGatt(connection);
    CharacteristicsFound(connection);
    NotifySessionStarted(connection);
    ConnectionResponseReceived(connection, selected_packet_size);
  }

  void RunWriteCharacteristicSuccessCallback() {
    EXPECT_FALSE(write_remote_characteristic_error_callback_.is_null());
    ASSERT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_success_callback_.Run();
  }

 protected:
  scoped_refptr<device::MockBluetoothAdapter> adapter_;
  RemoteDevice remote_device_;
  device::BluetoothUUID service_uuid_;
  device::BluetoothUUID tx_characteristic_uuid_;
  device::BluetoothUUID rx_characteristic_uuid_;
  std::unique_ptr<device::MockBluetoothDevice> mock_bluetooth_device_;
  std::unique_ptr<device::MockBluetoothGattService> service_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> tx_characteristic_;
  std::unique_ptr<device::MockBluetoothGattCharacteristic> rx_characteristic_;
  std::vector<uint8_t> last_value_written_on_tx_characteristic_;
  device::MockBluetoothGattNotifySession* notify_session_alias_;
  std::unique_ptr<MockBluetoothThrottler> bluetooth_throttler_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::MessageLoop message_loop_;
  bool last_wire_message_success_;
  std::shared_ptr<MockBluetoothLowEnergyWeavePacketGeneratorFactory>
      generator_factory_;
  std::shared_ptr<MockBluetoothLowEnergyWeavePacketReceiverFactory>
      receiver_factory_;
  MockConnectionObserver connection_observer_;

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

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       CreateAndDestroyWithoutConnectCallDoesntCrash) {
  BluetoothLowEnergyWeaveClientConnection connection(
      remote_device_, kTestRemoteDeviceBluetoothAddress, adapter_,
      service_uuid_, bluetooth_throttler_.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       DisconnectWithoutConnectDoesntCrash) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  Disconnect(connection.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectSuccess) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectSuccessDisconnect) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED);
  Disconnect(connection.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingCharacteristicsState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  Disconnect(connection.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingNotifySessionState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  Disconnect(connection.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectIncompleteDisconnectFromWaitingConnectionResponseState) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  Disconnect(connection.get());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsCharacteristicsNotFound) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());

  EXPECT_CALL(*rx_characteristic_, StartNotifySession(_, _)).Times(0);
  EXPECT_FALSE(characteristics_finder_success_callback_.is_null());
  ASSERT_FALSE(characteristics_finder_error_callback_.is_null());

  characteristics_finder_error_callback_.Run(
      {tx_characteristic_uuid_, kTXCharacteristicID},
      {rx_characteristic_uuid_, kRXCharacteristicID});

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsNotifySessionError) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _)).Times(0);
  EXPECT_FALSE(notify_session_success_callback_.is_null());
  ASSERT_FALSE(notify_session_error_callback_.is_null());

  notify_session_error_callback_.Run(
      device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectFailsErrorSendingConnectionRequest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  ConnectGatt(connection.get());
  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());

  // |connection| will call WriteRemoteCharacteristics(_,_) to try to send the
  // message |kMaxNumberOfTries| times. There is alredy one EXPECTA_CALL for
  // WriteRemoteCharacteristic(_,_,_) in NotifySessionStated, that's why we use
  // |kMaxNumberOfTries-1| in the EXPECT_CALL statement.
  EXPECT_EQ(0, connection_observer_.GetNumSendCompleted());
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .Times(kMaxNumberOfTries - 1)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kConnectionRequest);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_error_callback_.Run(
        device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveMessageSmallerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kSmallPackets0);

  EXPECT_EQ(received_bytes, kSmallMessage);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveMessageLargerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  std::string received_bytes;
  EXPECT_CALL(*connection, OnBytesReceived(_))
      .WillOnce(SaveArg<0>(&received_bytes));

  std::vector<Packet> packets = kLargePackets;

  for (auto packet : packets) {
    connection->GattCharacteristicValueChanged(
        adapter_.get(), rx_characteristic_.get(), packet);
  }
  EXPECT_EQ(received_bytes, kLargeMessage);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageSmallerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      base::MakeUnique<FakeWireMessage>(kSmallMessage, kTestFeature));

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kSmallPackets0);

  RunWriteCharacteristicSuccessCallback();

  EXPECT_EQ(1, connection_observer_.GetNumSendCompleted());
  EXPECT_EQ(kSmallMessage, connection_observer_.GetLastDeserializedMessage());
  EXPECT_TRUE(connection_observer_.GetLastSendSuccess());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageLargerThanCharacteristicSize) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  // Expecting a first call of WriteRemoteCharacteristic, after SendMessage is
  // called.
  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      base::MakeUnique<FakeWireMessage>(kLargeMessage, kTestFeature));

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kLargePackets0);
  std::vector<uint8_t> bytes_received(
      last_value_written_on_tx_characteristic_.begin() + 1,
      last_value_written_on_tx_characteristic_.end());

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();
  bytes_received.insert(bytes_received.end(),
                        last_value_written_on_tx_characteristic_.begin() + 1,
                        last_value_written_on_tx_characteristic_.end());

  std::vector<uint8_t> expected(kLargeMessage.begin(), kLargeMessage.end());
  EXPECT_EQ(expected, bytes_received);

  RunWriteCharacteristicSuccessCallback();

  EXPECT_EQ(1, connection_observer_.GetNumSendCompleted());
  EXPECT_EQ(kLargeMessage, connection_observer_.GetLastDeserializedMessage());
  EXPECT_TRUE(connection_observer_.GetLastSendSuccess());
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       SendMessageKeepsFailing) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .Times(kMaxNumberOfTries)
      .WillRepeatedly(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      base::MakeUnique<FakeWireMessage>(kSmallMessage, kTestFeature));

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_, kSmallPackets0);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());
    write_remote_characteristic_error_callback_.Run(
        device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
    if (i == kMaxNumberOfTries - 1) {
      EXPECT_EQ(1, connection_observer_.GetNumSendCompleted());
      EXPECT_EQ(kSmallMessage,
                connection_observer_.GetLastDeserializedMessage());
      EXPECT_FALSE(connection_observer_.GetLastSendSuccess());
    } else {
      EXPECT_EQ(0, connection_observer_.GetNumSendCompleted());
    }
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiveCloseConnectionTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());
  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kConnectionCloseUnknownError);

  EXPECT_EQ(receiver_factory_->GetMostRecentInstance()->GetReasonForClose(),
            ReasonForClose::UNKNOWN_ERROR);
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiverErrorTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  InitializeConnection(connection.get(), kDefaultMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kErroneousPacket);

  EXPECT_EQ(last_value_written_on_tx_characteristic_,
            kConnectionCloseApplicationError);
  EXPECT_EQ(receiver_factory_->GetMostRecentInstance()->GetReasonToClose(),
            ReasonForClose::APPLICATION_ERROR);

  RunWriteCharacteristicSuccessCallback();
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ReceiverErrorWithPendingWritesTest) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  InitializeConnection(connection.get(), kLargeMaxPacketSize);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  connection->SendMessage(
      base::MakeUnique<FakeWireMessage>(kLargeMessage, kTestFeature));

  connection->GattCharacteristicValueChanged(
      adapter_.get(), rx_characteristic_.get(), kErroneousPacket);

  EXPECT_EQ(last_value_written_on_tx_characteristic_, kLargePackets0);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));

  RunWriteCharacteristicSuccessCallback();

  EXPECT_EQ(last_value_written_on_tx_characteristic_,
            kConnectionCloseApplicationError);
  EXPECT_EQ(receiver_factory_->GetMostRecentInstance()->GetReasonToClose(),
            ReasonForClose::APPLICATION_ERROR);

  RunWriteCharacteristicSuccessCallback();
  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       WriteConnectionCloseMaxNumberOfTimes) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  InitializeConnection(connection.get(), kDefaultMaxPacketSize);
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED);

  EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
      .WillOnce(
          DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                SaveArg<1>(&write_remote_characteristic_success_callback_),
                SaveArg<2>(&write_remote_characteristic_error_callback_)));
  connection->Disconnect();
  EXPECT_EQ(connection->sub_status(), SubStatus::CONNECTED);

  for (int i = 0; i < kMaxNumberOfTries; i++) {
    EXPECT_EQ(last_value_written_on_tx_characteristic_,
              kConnectionCloseSuccess);
    ASSERT_FALSE(write_remote_characteristic_error_callback_.is_null());
    EXPECT_FALSE(write_remote_characteristic_success_callback_.is_null());

    if (i != kMaxNumberOfTries - 1) {
      EXPECT_CALL(*tx_characteristic_, WriteRemoteCharacteristic(_, _, _))
          .WillOnce(
              DoAll(SaveArg<0>(&last_value_written_on_tx_characteristic_),
                    SaveArg<1>(&write_remote_characteristic_success_callback_),
                    SaveArg<2>(&write_remote_characteristic_error_callback_)));
    }

    write_remote_characteristic_error_callback_.Run(
        device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  }

  EXPECT_EQ(connection->sub_status(), SubStatus::DISCONNECTED);
  EXPECT_EQ(connection->status(), Connection::DISCONNECTED);
}

TEST_F(CryptAuthBluetoothLowEnergyWeaveClientConnectionTest,
       ConnectAfterADelayWhenThrottled) {
  std::unique_ptr<TestBluetoothLowEnergyWeaveClientConnection> connection(
      CreateConnection());

  EXPECT_CALL(*bluetooth_throttler_, GetDelay())
      .WillOnce(Return(base::TimeDelta(base::TimeDelta::FromSeconds(1))));
  EXPECT_CALL(*mock_bluetooth_device_, CreateGattConnection(_, _))
      .WillOnce(DoAll(SaveArg<0>(&create_gatt_connection_success_callback_),
                      SaveArg<1>(&create_gatt_connection_error_callback_)));

  // No GATT connection should be created before the delay.
  connection->Connect();
  EXPECT_EQ(connection->sub_status(), SubStatus::WAITING_GATT_CONNECTION);
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
      base::MakeUnique<NiceMock<device::MockBluetoothGattConnection>>(
          adapter_, kTestRemoteDeviceBluetoothAddress));

  CharacteristicsFound(connection.get());
  NotifySessionStarted(connection.get());
  ConnectionResponseReceived(connection.get(), kDefaultMaxPacketSize);
}

}  // namespace weave

}  // namespace cryptauth
