// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_connection_manager.h"

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/ble_constants.h"
#include "chromeos/components/tether/fake_ble_advertiser.h"
#include "chromeos/components/tether/fake_ble_scanner.h"
#include "chromeos/components/tether/proto/tether.pb.h"
#include "chromeos/components/tether/timer_factory.h"
#include "components/cryptauth/ble/bluetooth_low_energy_weave_client_connection.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/cryptauth/fake_secure_channel.h"
#include "components/cryptauth/fake_secure_message_delegate.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace chromeos {

namespace tether {

namespace {

const char kTetherFeature[] = "magic_tether";

const char kUserId[] = "userId";

const char kBluetoothAddress1[] = "11:22:33:44:55:66";
const char kBluetoothAddress2[] = "22:33:44:55:66:77";
const char kBluetoothAddress3[] = "33:44:55:66:77:88";

constexpr base::TimeDelta kStatusConnectedTime =
    base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kStatusAuthenticatedTime =
    base::TimeDelta::FromSeconds(3);

struct SecureChannelStatusChange {
  SecureChannelStatusChange(const cryptauth::RemoteDevice& remote_device,
                            const cryptauth::SecureChannel::Status& old_status,
                            const cryptauth::SecureChannel::Status& new_status)
      : remote_device(remote_device),
        old_status(old_status),
        new_status(new_status) {}

  cryptauth::RemoteDevice remote_device;
  cryptauth::SecureChannel::Status old_status;
  cryptauth::SecureChannel::Status new_status;
};

struct ReceivedMessage {
  ReceivedMessage(const cryptauth::RemoteDevice& remote_device,
                  const std::string& payload)
      : remote_device(remote_device), payload(payload) {}

  cryptauth::RemoteDevice remote_device;
  std::string payload;
};

class MockTimerFactory : public TimerFactory {
 public:
  std::unique_ptr<base::Timer> CreateOneShotTimer() override {
    return base::MakeUnique<base::MockTimer>(false /* retains_user_task */,
                                             false /* is_repeating */);
  }
};

// Observer used in all tests except for ObserverUnregisters() which tracks all
// status changes and messages received.
class TestObserver final : public BleConnectionManager::Observer {
 public:
  TestObserver() {}

  // BleConnectionManager::Observer:
  void OnSecureChannelStatusChanged(
      const cryptauth::RemoteDevice& remote_device,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status) override {
    connection_status_changes_.push_back(
        SecureChannelStatusChange(remote_device, old_status, new_status));
  }

  void OnMessageReceived(const cryptauth::RemoteDevice& remote_device,
                         const std::string& payload) override {
    received_messages_.push_back(ReceivedMessage(remote_device, payload));
  }

  void OnMessageSent(int sequence_number) override {
    sent_sequence_numbers_.push_back(sequence_number);
  }

  const std::vector<SecureChannelStatusChange>& connection_status_changes() {
    return connection_status_changes_;
  }

  const std::vector<ReceivedMessage>& received_messages() {
    return received_messages_;
  }

  const std::vector<int>& sent_sequence_numbers() {
    return sent_sequence_numbers_;
  }

 private:
  std::vector<SecureChannelStatusChange> connection_status_changes_;
  std::vector<ReceivedMessage> received_messages_;
  std::vector<int> sent_sequence_numbers_;
};

// Observer used in ObserverUnregisters() which unregisters a device when it
// receives a callback from the manager. The device it unregisters is the same
// one it receives the event about.
class UnregisteringObserver : public BleConnectionManager::Observer {
 public:
  UnregisteringObserver(BleConnectionManager* manager,
                        MessageType connection_reason)
      : manager_(manager), connection_reason_(connection_reason) {}

  // BleConnectionManager::Observer:
  void OnSecureChannelStatusChanged(
      const cryptauth::RemoteDevice& remote_device,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status) override {
    manager_->UnregisterRemoteDevice(remote_device, connection_reason_);
  }

  void OnMessageReceived(const cryptauth::RemoteDevice& remote_device,
                         const std::string& payload) override {
    manager_->UnregisterRemoteDevice(remote_device, connection_reason_);
  }

  void OnMessageSent(int sequence_number) override { NOTIMPLEMENTED(); }

 private:
  BleConnectionManager* manager_;
  MessageType connection_reason_;
};

class FakeConnectionWithAddress : public cryptauth::FakeConnection {
 public:
  FakeConnectionWithAddress(const cryptauth::RemoteDevice& remote_device,
                            const std::string& device_address)
      : FakeConnection(remote_device, false /* should_auto_connect */),
        device_address_(device_address) {}

  // cryptauth::Connection:
  std::string GetDeviceAddress() override { return device_address_; }

 private:
  const std::string device_address_;
};

class FakeConnectionFactory final
    : public cryptauth::weave::BluetoothLowEnergyWeaveClientConnection::
          Factory {
 public:
  FakeConnectionFactory(
      scoped_refptr<device::BluetoothAdapter> expected_adapter,
      const device::BluetoothUUID expected_remote_service_uuid)
      : expected_adapter_(expected_adapter),
        expected_remote_service_uuid_(expected_remote_service_uuid) {}

  std::unique_ptr<cryptauth::Connection> BuildInstance(
      const cryptauth::RemoteDevice& remote_device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      const device::BluetoothUUID remote_service_uuid,
      device::BluetoothDevice* bluetooth_device,
      bool should_set_low_connection_latency) override {
    EXPECT_EQ(expected_adapter_, adapter);
    EXPECT_EQ(expected_remote_service_uuid_, remote_service_uuid);
    EXPECT_FALSE(should_set_low_connection_latency);

    return base::WrapUnique<FakeConnectionWithAddress>(
        new FakeConnectionWithAddress(remote_device,
                                      bluetooth_device->GetAddress()));
  }

 private:
  scoped_refptr<device::BluetoothAdapter> expected_adapter_;
  const device::BluetoothUUID expected_remote_service_uuid_;
};

std::vector<cryptauth::RemoteDevice> CreateTestDevices(size_t num_to_create) {
  std::vector<cryptauth::RemoteDevice> test_devices =
      cryptauth::GenerateTestRemoteDevices(num_to_create);
  for (auto& device : test_devices) {
    device.user_id = kUserId;
  }
  return test_devices;
}

}  // namespace

class BleConnectionManagerTest : public testing::Test {
 protected:
  class FakeSecureChannel : public cryptauth::FakeSecureChannel {
   public:
    FakeSecureChannel(std::unique_ptr<cryptauth::Connection> connection,
                      cryptauth::CryptAuthService* cryptauth_service)
        : cryptauth::FakeSecureChannel(std::move(connection),
                                       cryptauth_service) {}
    ~FakeSecureChannel() override {}

    void AddObserver(Observer* observer) override {
      cryptauth::FakeSecureChannel::AddObserver(observer);

      EXPECT_EQ(static_cast<size_t>(1), observers().size());
    }

    void RemoveObserver(Observer* observer) override {
      cryptauth::FakeSecureChannel::RemoveObserver(observer);
      EXPECT_EQ(static_cast<size_t>(0), observers().size());
    }
  };

  class FakeSecureChannelFactory final
      : public cryptauth::SecureChannel::Factory {
   public:
    FakeSecureChannelFactory() {}

    void SetExpectedDeviceAddress(const std::string& expected_device_address) {
      expected_device_address_ = expected_device_address;
    }

    std::unique_ptr<cryptauth::SecureChannel> BuildInstance(
        std::unique_ptr<cryptauth::Connection> connection,
        cryptauth::CryptAuthService* cryptauth_service) override {
      FakeConnectionWithAddress* fake_connection =
          static_cast<FakeConnectionWithAddress*>(connection.get());
      EXPECT_EQ(expected_device_address_, fake_connection->GetDeviceAddress());
      return base::WrapUnique(
          new FakeSecureChannel(std::move(connection), cryptauth_service));
    }

   private:
    std::string expected_device_address_;
  };

  BleConnectionManagerTest() : test_devices_(CreateTestDevices(4)) {
    // These tests assume a maximum of two concurrent advertisers. Some of the
    // multi-device tests would need to be re-written if this constant changes.
    EXPECT_EQ(2u, kMaxConcurrentAdvertisements);
  }

  void SetUp() override {
    verified_status_changes_.clear();
    verified_received_messages_.clear();

    fake_cryptauth_service_ =
        base::MakeUnique<cryptauth::FakeCryptAuthService>();
    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

    device_queue_ = base::MakeUnique<BleAdvertisementDeviceQueue>();

    fake_ble_advertiser_ = base::MakeUnique<FakeBleAdvertiser>(
        true /* automatically_update_active_advertisements */);

    fake_ble_scanner_ = base::MakeUnique<FakeBleScanner>(
        true /* automatically_update_discovery_session */);

    fake_connection_factory_ = base::WrapUnique(new FakeConnectionFactory(
        mock_adapter_, device::BluetoothUUID(kGattServerUuid)));
    cryptauth::weave::BluetoothLowEnergyWeaveClientConnection::Factory::
        SetInstanceForTesting(fake_connection_factory_.get());

    fake_secure_channel_factory_ =
        base::WrapUnique(new FakeSecureChannelFactory());
    cryptauth::SecureChannel::Factory::SetInstanceForTesting(
        fake_secure_channel_factory_.get());

    manager_ = base::WrapUnique(new BleConnectionManager(
        fake_cryptauth_service_.get(), mock_adapter_, device_queue_.get(),
        fake_ble_advertiser_.get(), fake_ble_scanner_.get()));
    test_observer_ = base::WrapUnique(new TestObserver());
    manager_->AddObserver(test_observer_.get());

    test_clock_ = new base::SimpleTestClock();
    test_clock_->SetNow(base::Time::UnixEpoch());
    mock_timer_factory_ = new MockTimerFactory();
    manager_->SetTestDoubles(base::WrapUnique(test_clock_),
                             base::WrapUnique(mock_timer_factory_));
  }

  void TearDown() override {
    // All state changes should have already been verified. This ensures that
    // no test has missed one.
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>());

    // Same with received messages.
    VerifyReceivedMessages(std::vector<ReceivedMessage>());
  }

  void VerifyConnectionStateChanges(
      const std::vector<SecureChannelStatusChange>& expected_changes) {
    verified_status_changes_.insert(verified_status_changes_.end(),
                                    expected_changes.begin(),
                                    expected_changes.end());

    ASSERT_EQ(verified_status_changes_.size(),
              test_observer_->connection_status_changes().size());

    for (size_t i = 0; i < verified_status_changes_.size(); i++) {
      EXPECT_EQ(verified_status_changes_[i].remote_device,
                test_observer_->connection_status_changes()[i].remote_device);
      EXPECT_EQ(verified_status_changes_[i].old_status,
                test_observer_->connection_status_changes()[i].old_status);
      EXPECT_EQ(verified_status_changes_[i].new_status,
                test_observer_->connection_status_changes()[i].new_status);
    }
  }

  void VerifyReceivedMessages(
      const std::vector<ReceivedMessage>& expected_messages) {
    verified_received_messages_.insert(verified_received_messages_.end(),
                                       expected_messages.begin(),
                                       expected_messages.end());

    ASSERT_EQ(verified_received_messages_.size(),
              test_observer_->received_messages().size());

    for (size_t i = 0; i < verified_received_messages_.size(); i++) {
      EXPECT_EQ(verified_received_messages_[i].remote_device,
                test_observer_->received_messages()[i].remote_device);
      EXPECT_EQ(verified_received_messages_[i].payload,
                test_observer_->received_messages()[i].payload);
    }
  }

  void VerifyNoTimeoutSet(const cryptauth::RemoteDevice& remote_device) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_TRUE(connection_metadata);
    EXPECT_FALSE(
        connection_metadata->connection_attempt_timeout_timer_->IsRunning());
  }

  void VerifyFailImmediatelyTimeoutSet(
      const cryptauth::RemoteDevice& remote_device) {
    VerifyTimeoutSet(remote_device,
                     BleConnectionManager::kFailImmediatelyTimeoutMillis);
  }

  void VerifyAdvertisingTimeoutSet(
      const cryptauth::RemoteDevice& remote_device) {
    VerifyTimeoutSet(remote_device,
                     BleConnectionManager::kAdvertisingTimeoutMillis);
  }

  void VerifyTimeoutSet(const cryptauth::RemoteDevice& remote_device,
                        int64_t expected_num_millis) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_TRUE(connection_metadata);
    EXPECT_TRUE(
        connection_metadata->connection_attempt_timeout_timer_->IsRunning());
    EXPECT_EQ(base::TimeDelta::FromMilliseconds(expected_num_millis),
              connection_metadata->connection_attempt_timeout_timer_
                  ->GetCurrentDelay());
  }

  void FireTimerForDevice(const cryptauth::RemoteDevice& remote_device) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_TRUE(connection_metadata);
    EXPECT_TRUE(
        connection_metadata->connection_attempt_timeout_timer_->IsRunning());
    static_cast<base::MockTimer*>(
        connection_metadata->connection_attempt_timeout_timer_.get())
        ->Fire();
  }

  void NotifyReceivedAdvertisementFromDevice(
      const std::string& bluetooth_address,
      const cryptauth::RemoteDevice& remote_device) {
    device::MockBluetoothDevice device(
        mock_adapter_.get(), 0u /* bluetooth_class */, "name",
        bluetooth_address, false /* paired */, false /* connected */);
    fake_ble_scanner_->NotifyReceivedAdvertisementFromDevice(remote_device,
                                                             &device);
  }

  FakeSecureChannel* GetChannelForDevice(
      const cryptauth::RemoteDevice& remote_device) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_TRUE(connection_metadata);
    EXPECT_TRUE(connection_metadata->secure_channel_);
    return static_cast<FakeSecureChannel*>(
        connection_metadata->secure_channel_.get());
  }

  void VerifyDeviceRegistered(const cryptauth::RemoteDevice& remote_device) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_TRUE(connection_metadata);
  }

  void VerifyDeviceNotRegistered(const cryptauth::RemoteDevice& remote_device) {
    BleConnectionManager::ConnectionMetadata* connection_metadata =
        manager_->GetConnectionMetadata(remote_device);
    EXPECT_FALSE(connection_metadata);
  }

  // Registers |remote_device|, creates a connection to that device at
  // |bluetooth_address|, and authenticates the resulting channel.
  FakeSecureChannel* ConnectSuccessfully(
      const cryptauth::RemoteDevice& remote_device,
      const std::string& bluetooth_address,
      const MessageType connection_reason) {
    test_clock_->SetNow(base::Time::UnixEpoch());

    manager_->RegisterRemoteDevice(remote_device, connection_reason);
    VerifyAdvertisingTimeoutSet(remote_device);
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
        {remote_device, cryptauth::SecureChannel::Status::DISCONNECTED,
         cryptauth::SecureChannel::Status::CONNECTING}});

    FakeSecureChannel* channel =
        ConnectChannel(remote_device, bluetooth_address);
    AuthenticateChannel(remote_device);
    return channel;
  }

  // Creates a connection to |remote_device| at |bluetooth_address|. The device
  // must be registered before calling this function.
  FakeSecureChannel* ConnectChannel(
      const cryptauth::RemoteDevice& remote_device,
      const std::string& bluetooth_address) {
    VerifyDeviceRegistered(remote_device);

    fake_secure_channel_factory_->SetExpectedDeviceAddress(bluetooth_address);
    NotifyReceivedAdvertisementFromDevice(bluetooth_address, remote_device);
    return GetChannelForDevice(remote_device);
  }

  // Authenticates the SecureChannel associated with |remote_device|. The device
  // must be registered and already have an associated channel before calling
  // this function.
  void AuthenticateChannel(const cryptauth::RemoteDevice& remote_device) {
    VerifyDeviceRegistered(remote_device);

    FakeSecureChannel* channel = GetChannelForDevice(remote_device);
    DCHECK(channel);

    num_expected_authenticated_channels_++;

    test_clock_->SetNow(base::Time::UnixEpoch());
    channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTING);

    test_clock_->Advance(kStatusConnectedTime);
    channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTED);
    histogram_tester_.ExpectTimeBucketCount(
        "InstantTethering.Performance.AdvertisementToConnectionDuration",
        kStatusConnectedTime, num_expected_authenticated_channels_);

    test_clock_->Advance(kStatusAuthenticatedTime);
    channel->ChangeStatus(cryptauth::SecureChannel::Status::AUTHENTICATING);
    channel->ChangeStatus(cryptauth::SecureChannel::Status::AUTHENTICATED);
    histogram_tester_.ExpectTimeBucketCount(
        "InstantTethering.Performance.ConnectionToAuthenticationDuration",
        kStatusAuthenticatedTime, num_expected_authenticated_channels_);

    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
        {remote_device, cryptauth::SecureChannel::Status::CONNECTING,
         cryptauth::SecureChannel::Status::CONNECTED},
        {remote_device, cryptauth::SecureChannel::Status::CONNECTED,
         cryptauth::SecureChannel::Status::AUTHENTICATING},
        {remote_device, cryptauth::SecureChannel::Status::AUTHENTICATING,
         cryptauth::SecureChannel::Status::AUTHENTICATED}});
  }

  void VerifyLastMessageSent(FakeSecureChannel* channel,
                             int sequence_number,
                             const std::string& payload,
                             size_t expected_size) {
    ASSERT_EQ(expected_size, channel->sent_messages().size());
    cryptauth::FakeSecureChannel::SentMessage sent_message =
        channel->sent_messages()[expected_size - 1];
    EXPECT_EQ(kTetherFeature, sent_message.feature);
    EXPECT_EQ(payload, sent_message.payload);

    std::vector<int> sent_sequence_numbers_before_finished =
        test_observer_->sent_sequence_numbers();
    channel->CompleteSendingMessage(sequence_number);
    std::vector<int> sent_sequence_numbers_after_finished =
        test_observer_->sent_sequence_numbers();
    EXPECT_EQ(sent_sequence_numbers_before_finished.size() + 1u,
              sent_sequence_numbers_after_finished.size());
    EXPECT_EQ(sequence_number, sent_sequence_numbers_after_finished.back());
  }

  void VerifyAdvertisingToConnectionDurationMetricNotRecorded() {
    histogram_tester_.ExpectTotalCount(
        "InstantTethering.Performance.AdvertisementToConnectionDuration", 0);
  }

  void VerifyConnectionToAuthenticationDurationMetricNotRecorded() {
    histogram_tester_.ExpectTotalCount(
        "InstantTethering.Performance.ConnectionToAuthenticationDuration", 0);
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<cryptauth::FakeCryptAuthService> fake_cryptauth_service_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<FakeBleAdvertiser> fake_ble_advertiser_;
  std::unique_ptr<FakeBleScanner> fake_ble_scanner_;
  std::unique_ptr<BleAdvertisementDeviceQueue> device_queue_;
  MockTimerFactory* mock_timer_factory_;
  base::SimpleTestClock* test_clock_;
  std::unique_ptr<FakeConnectionFactory> fake_connection_factory_;
  std::unique_ptr<FakeSecureChannelFactory> fake_secure_channel_factory_;
  std::unique_ptr<TestObserver> test_observer_;

  std::vector<SecureChannelStatusChange> verified_status_changes_;
  std::vector<ReceivedMessage> verified_received_messages_;

  std::unique_ptr<BleConnectionManager> manager_;

  int num_expected_authenticated_channels_ = 0;
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BleConnectionManagerTest);
};

TEST_F(BleConnectionManagerTest, TestCannotScan) {
  fake_ble_scanner_->set_should_fail_to_register(true);

  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestCannotAdvertise) {
  fake_ble_advertiser_->set_should_fail_to_start_advertising(true);

  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestRegistersButNoResult) {
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestRegistersAndUnregister_NoConnection) {
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestRegisterWithNoConnection_TimeoutOccurs) {
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  FireTimerForDevice(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestSuccessfulConnection_FailsAuthentication) {
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  fake_secure_channel_factory_->SetExpectedDeviceAddress(kBluetoothAddress1);
  NotifyReceivedAdvertisementFromDevice(kBluetoothAddress1, test_devices_[0]);
  FakeSecureChannel* channel = GetChannelForDevice(test_devices_[0]);

  // Should not result in an additional "disconnected => connecting" broadcast.
  channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTING);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>());

  channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::CONNECTED}});

  channel->ChangeStatus(cryptauth::SecureChannel::Status::AUTHENTICATING);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED,
       cryptauth::SecureChannel::Status::AUTHENTICATING}});

  // Fail authentication, which should automatically start a retry.
  channel->ChangeStatus(cryptauth::SecureChannel::Status::DISCONNECTED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TestSuccessfulConnection_SendAndReceive) {
  FakeSecureChannel* channel =
      ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                          MessageType::TETHER_AVAILABILITY_REQUEST);

  int sequence_number = manager_->SendMessage(test_devices_[0], "request1");
  VerifyLastMessageSent(channel, sequence_number, "request1", 1);

  channel->ReceiveMessage(kTetherFeature, "response1");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response1"}});

  sequence_number = manager_->SendMessage(test_devices_[0], "request2");
  VerifyLastMessageSent(channel, sequence_number, "request2", 2);

  channel->ReceiveMessage(kTetherFeature, "response2");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response2"}});

  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[0]);
}

// Test for fix to crbug.com/706640. This test will crash without the fix.
TEST_F(BleConnectionManagerTest,
       TestSuccessfulConnection_MultipleAdvertisementsReceived) {
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  fake_secure_channel_factory_->SetExpectedDeviceAddress(kBluetoothAddress1);

  // Simulate multiple advertisements being received:
  NotifyReceivedAdvertisementFromDevice(kBluetoothAddress1, test_devices_[0]);
  FakeSecureChannel* channel = GetChannelForDevice(test_devices_[0]);

  NotifyReceivedAdvertisementFromDevice(kBluetoothAddress1, test_devices_[0]);
  // Verify that a new channel has not been created:
  EXPECT_EQ(channel, GetChannelForDevice(test_devices_[0]));

  NotifyReceivedAdvertisementFromDevice(kBluetoothAddress1, test_devices_[0]);
  // Verify that a new channel has not been created:
  EXPECT_EQ(channel, GetChannelForDevice(test_devices_[0]));
}

TEST_F(BleConnectionManagerTest,
       TestSuccessfulConnection_MultipleConnectionReasons) {
  ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                      MessageType::TETHER_AVAILABILITY_REQUEST);

  // Now, register a different connection reason.
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::CONNECT_TETHERING_REQUEST);

  // Unregister the |TETHER_AVAILABILITY_REQUEST| reason, but leave the
  // |CONNECT_TETHERING_REQUEST| registered.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyDeviceRegistered(test_devices_[0]);

  // Now, unregister the other reason; this should cause the device to be
  // fully unregistered.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::CONNECT_TETHERING_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[0]);
}

TEST_F(BleConnectionManagerTest, TestGetStatusForDevice) {
  cryptauth::SecureChannel::Status status;

  // Should return false when the device has not yet been registered at all.
  EXPECT_FALSE(manager_->GetStatusForDevice(test_devices_[0], &status));

  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Should be CONNECTING at this point.
  EXPECT_TRUE(manager_->GetStatusForDevice(test_devices_[0], &status));
  EXPECT_EQ(cryptauth::SecureChannel::Status::CONNECTING, status);

  fake_secure_channel_factory_->SetExpectedDeviceAddress(kBluetoothAddress1);
  NotifyReceivedAdvertisementFromDevice(kBluetoothAddress1, test_devices_[0]);
  FakeSecureChannel* channel = GetChannelForDevice(test_devices_[0]);

  channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTING);
  EXPECT_TRUE(manager_->GetStatusForDevice(test_devices_[0], &status));
  EXPECT_EQ(cryptauth::SecureChannel::Status::CONNECTING, status);

  channel->ChangeStatus(cryptauth::SecureChannel::Status::CONNECTED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::CONNECTED}});
  EXPECT_TRUE(manager_->GetStatusForDevice(test_devices_[0], &status));
  EXPECT_EQ(cryptauth::SecureChannel::Status::CONNECTED, status);

  channel->ChangeStatus(cryptauth::SecureChannel::Status::AUTHENTICATING);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTED,
       cryptauth::SecureChannel::Status::AUTHENTICATING}});
  EXPECT_TRUE(manager_->GetStatusForDevice(test_devices_[0], &status));
  EXPECT_EQ(cryptauth::SecureChannel::Status::AUTHENTICATING, status);

  channel->ChangeStatus(cryptauth::SecureChannel::Status::AUTHENTICATED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATING,
       cryptauth::SecureChannel::Status::AUTHENTICATED}});
  EXPECT_TRUE(manager_->GetStatusForDevice(test_devices_[0], &status));
  EXPECT_EQ(cryptauth::SecureChannel::Status::AUTHENTICATED, status);

  // Now, unregister the device and check that GetStatusForDevice() once again
  // returns false.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  EXPECT_FALSE(manager_->GetStatusForDevice(test_devices_[0], &status));
}

TEST_F(BleConnectionManagerTest,
       TestSuccessfulConnection_DisconnectsAfterConnection) {
  FakeSecureChannel* channel =
      ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                          MessageType::TETHER_AVAILABILITY_REQUEST);

  channel->ChangeStatus(cryptauth::SecureChannel::Status::DISCONNECTED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});
}

TEST_F(BleConnectionManagerTest, TwoDevices_NeitherCanScan) {
  fake_ble_scanner_->set_should_fail_to_register(true);
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  manager_->RegisterRemoteDevice(test_devices_[1],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});
}

TEST_F(BleConnectionManagerTest, TwoDevices_NeitherCanAdvertise) {
  fake_ble_advertiser_->set_should_fail_to_start_advertising(true);

  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  manager_->RegisterRemoteDevice(test_devices_[1],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyFailImmediatelyTimeoutSet(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest,
       TwoDevices_RegisterWithNoConnection_TimerFires) {
  // Register device 0.
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Register device 1.
  manager_->RegisterRemoteDevice(test_devices_[1],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Simulate timeout for device 0 by firing timeout.
  FireTimerForDevice(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Simulate timeout for device 1 by firing timeout.
  FireTimerForDevice(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Unregister device 0.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  // Unregister device 1.
  manager_->UnregisterRemoteDevice(test_devices_[1],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  VerifyAdvertisingToConnectionDurationMetricNotRecorded();
  VerifyConnectionToAuthenticationDurationMetricNotRecorded();
}

TEST_F(BleConnectionManagerTest, TwoDevices_OneConnects) {
  // Successfully connect to device 0.
  ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                      MessageType::TETHER_AVAILABILITY_REQUEST);

  // Register device 1.
  manager_->RegisterRemoteDevice(test_devices_[1],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyAdvertisingTimeoutSet(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Simulate timeout for device 1 by firing timeout.
  FireTimerForDevice(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Unregister device 0.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  // Unregister device 1.
  manager_->UnregisterRemoteDevice(test_devices_[1],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
}

TEST_F(BleConnectionManagerTest, TwoDevices_BothConnectSendAndReceive) {
  FakeSecureChannel* channel0 =
      ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                          MessageType::TETHER_AVAILABILITY_REQUEST);

  FakeSecureChannel* channel1 =
      ConnectSuccessfully(test_devices_[1], kBluetoothAddress2,
                          MessageType::TETHER_AVAILABILITY_REQUEST);

  int sequence_number =
      manager_->SendMessage(test_devices_[0], "request1_device0");
  VerifyLastMessageSent(channel0, sequence_number, "request1_device0", 1);

  sequence_number = manager_->SendMessage(test_devices_[1], "request1_device1");
  VerifyLastMessageSent(channel1, sequence_number, "request1_device1", 1);

  channel0->ReceiveMessage(kTetherFeature, "response1_device0");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response1_device0"}});

  channel1->ReceiveMessage(kTetherFeature, "response1_device1");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[1], "response1_device1"}});

  sequence_number = manager_->SendMessage(test_devices_[0], "request2_device0");
  VerifyLastMessageSent(channel0, sequence_number, "request2_device0", 2);

  sequence_number = manager_->SendMessage(test_devices_[1], "request2_device1");
  VerifyLastMessageSent(channel1, sequence_number, "request2_device1", 2);

  channel0->ReceiveMessage(kTetherFeature, "response2_device0");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response2_device0"}});

  channel1->ReceiveMessage(kTetherFeature, "response2_device1");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[1], "response2_device1"}});

  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[0]);

  manager_->UnregisterRemoteDevice(test_devices_[1],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[1]);
}

TEST_F(BleConnectionManagerTest, FourDevices_ComprehensiveTest) {
  // Register all devices. Since the maximum number of simultaneous connection
  // attempts is 2, only devices 0 and 1 should actually start connecting.
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  manager_->RegisterRemoteDevice(test_devices_[1],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  manager_->RegisterRemoteDevice(test_devices_[2],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);
  manager_->RegisterRemoteDevice(test_devices_[3],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);

  // Devices 0 and 1 should be advertising; devices 2 and 3 should not be.
  VerifyAdvertisingTimeoutSet(test_devices_[0]);
  VerifyAdvertisingTimeoutSet(test_devices_[1]);
  VerifyNoTimeoutSet(test_devices_[2]);
  VerifyNoTimeoutSet(test_devices_[3]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING},
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Device 0 connects successfully.
  FakeSecureChannel* channel0 =
      ConnectChannel(test_devices_[0], kBluetoothAddress1);

  // Since device 0 has connected, advertising to that device is no longer
  // necessary. Device 2 should have filled up that advertising slot.
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[2], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Meanwhile, device 1 fails to connect, so the timeout fires. The advertising
  // slot left by device 1 creates space for device 3 to start connecting.
  FireTimerForDevice(test_devices_[1]);
  VerifyAdvertisingTimeoutSet(test_devices_[3]);
  VerifyNoTimeoutSet(test_devices_[1]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[3], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Now, device 0 authenticates and sends and receives a message.
  AuthenticateChannel(test_devices_[0]);
  int sequence_number = manager_->SendMessage(test_devices_[0], "request1");
  VerifyLastMessageSent(channel0, sequence_number, "request1", 1);

  channel0->ReceiveMessage(kTetherFeature, "response1");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response1"}});

  // Now, device 0 is unregistered.
  manager_->UnregisterRemoteDevice(test_devices_[0],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyDeviceNotRegistered(test_devices_[0]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});

  // Device 2 fails to connect, so the timeout fires. Device 1 takes its spot.
  FireTimerForDevice(test_devices_[2]);
  VerifyAdvertisingTimeoutSet(test_devices_[1]);
  VerifyNoTimeoutSet(test_devices_[2]);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[2], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[1], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Device 3 connects successfully.
  FakeSecureChannel* channel3 =
      ConnectChannel(test_devices_[3], kBluetoothAddress3);

  // Since device 3 has connected, device 2 starts connecting again.
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[2], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING}});

  // Now, device 3 authenticates and sends and receives a message.
  AuthenticateChannel(test_devices_[3]);
  sequence_number = manager_->SendMessage(test_devices_[3], "request3");
  VerifyLastMessageSent(channel3, sequence_number, "request3", 1);

  channel3->ReceiveMessage(kTetherFeature, "response3");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[3], "response3"}});

  // Assume that none of the other devices can connect, and unregister the
  // remaining 3 devices.
  manager_->UnregisterRemoteDevice(test_devices_[3],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyDeviceNotRegistered(test_devices_[3]);
  manager_->UnregisterRemoteDevice(test_devices_[1],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyDeviceNotRegistered(test_devices_[1]);
  manager_->UnregisterRemoteDevice(test_devices_[2],
                                   MessageType::TETHER_AVAILABILITY_REQUEST);
  VerifyDeviceNotRegistered(test_devices_[2]);

  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[3], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[1], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED},
      {test_devices_[2], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
}

// Regression test for crbug.com/733360. This bug caused a crash when there were
// multiple observers of BleConnectionManager. The bug was that when an event
// occurred which triggered observers to be notified (i.e., a status changed or
// message received event), sometimes one of the observers would unregister the
// device, which, in turn, caused the associated ConnectionMetadata to be
// deleted. Then, the next observer would be notified of the event after the
// deletion. Since the parameters to the observer callbacks were passed by
// reference (and the reference was held by the deleted ConnectionMetadata),
// this led to the second observer referencing deleted memory. The fix is to
// pass the parameters from the ConnectionMetadata to BleConnectionManager by
// value instead.
TEST_F(BleConnectionManagerTest, ObserverUnregisters) {
  FakeSecureChannel* channel =
      ConnectSuccessfully(test_devices_[0], kBluetoothAddress1,
                          MessageType::TETHER_AVAILABILITY_REQUEST);

  // Register two separate UnregisteringObservers. When a message is received,
  // the first observer will unregister the device.
  UnregisteringObserver first(manager_.get(),
                              MessageType::TETHER_AVAILABILITY_REQUEST);
  UnregisteringObserver second(manager_.get(),
                               MessageType::TETHER_AVAILABILITY_REQUEST);
  manager_->AddObserver(&first);
  manager_->AddObserver(&second);

  // Receive a message over the channel. This should invoke the observer
  // callbacks. This would have caused a crash before the fix for
  // crbug.com/733360.
  channel->ReceiveMessage(kTetherFeature, "response1");
  VerifyReceivedMessages(
      std::vector<ReceivedMessage>{{test_devices_[0], "response1"}});

  // We expect the device to be unregistered (by the observer).
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::AUTHENTICATED,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[0]);

  // Now, register the device again. This should cause a "disconnected =>
  // connecting" status change. This time, the multiple observers will respond
  // to a status change event instead of a message received event. This also
  // would have caused a crash before the fix for crbug.com/733360.
  manager_->RegisterRemoteDevice(test_devices_[0],
                                 MessageType::TETHER_AVAILABILITY_REQUEST);

  // We expect the device to be unregistered (by the observer).
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>{
      {test_devices_[0], cryptauth::SecureChannel::Status::DISCONNECTED,
       cryptauth::SecureChannel::Status::CONNECTING},
      {test_devices_[0], cryptauth::SecureChannel::Status::CONNECTING,
       cryptauth::SecureChannel::Status::DISCONNECTED}});
  VerifyDeviceNotRegistered(test_devices_[0]);
}

}  // namespace tether

}  // namespace chromeos
