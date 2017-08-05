// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/secure_channel.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/fake_authenticator.h"
#include "components/cryptauth/fake_connection.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/cryptauth/fake_secure_context.h"
#include "components/cryptauth/fake_secure_message_delegate.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/cryptauth/wire_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptauth {

namespace {

const std::string test_user_id = "testUserId";

struct SecureChannelStatusChange {
  SecureChannelStatusChange(
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status)
      : old_status(old_status), new_status(new_status) {}

  SecureChannel::Status old_status;
  SecureChannel::Status new_status;
};

struct ReceivedMessage {
  ReceivedMessage(const std::string& feature, const std::string& payload)
      : feature(feature), payload(payload) {}

  std::string feature;
  std::string payload;
};

class TestObserver : public SecureChannel::Observer {
 public:
  TestObserver(SecureChannel* secure_channel)
      : secure_channel_(secure_channel) {}

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override {
    DCHECK(secure_channel == secure_channel_);
    connection_status_changes_.push_back(
        SecureChannelStatusChange(old_status, new_status));
  }

  void OnMessageReceived(SecureChannel* secure_channel,
                         const std::string& feature,
                         const std::string& payload) override {
    DCHECK(secure_channel == secure_channel_);
    received_messages_.push_back(ReceivedMessage(feature, payload));
  }

  void OnMessageSent(SecureChannel* secure_channel,
                     int sequence_number) override {
    DCHECK(secure_channel == secure_channel_);
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
  SecureChannel* secure_channel_;
  std::vector<SecureChannelStatusChange> connection_status_changes_;
  std::vector<ReceivedMessage> received_messages_;
  std::vector<int> sent_sequence_numbers_;
};

// Observer used in the ObserverDeletesChannel test. This Observer deletes the
// SecureChannel when it receives an OnMessageSent() call.
class DeletingObserver : public SecureChannel::Observer {
 public:
  DeletingObserver(std::unique_ptr<SecureChannel>* secure_channel)
      : secure_channel_(secure_channel) {}

  // SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      SecureChannel* secure_channel,
      const SecureChannel::Status& old_status,
      const SecureChannel::Status& new_status) override {}

  void OnMessageReceived(SecureChannel* secure_channel,
                         const std::string& feature,
                         const std::string& payload) override {}

  void OnMessageSent(SecureChannel* secure_channel,
                     int sequence_number) override {
    DCHECK(secure_channel == secure_channel_->get());
    // Delete the channel when an OnMessageSent() call occurs.
    secure_channel_->reset();
  }

 private:
  std::unique_ptr<SecureChannel>* secure_channel_;
};

class TestAuthenticatorFactory : public DeviceToDeviceAuthenticator::Factory {
 public:
  TestAuthenticatorFactory() : last_instance_(nullptr) {}

  std::unique_ptr<Authenticator> BuildInstance(
      cryptauth::Connection* connection,
      const std::string& account_id,
      std::unique_ptr<cryptauth::SecureMessageDelegate>
          secure_message_delegate) override {
    last_instance_ = new FakeAuthenticator();
    return base::WrapUnique(last_instance_);
  }

  Authenticator* last_instance() {
    return last_instance_;
  }

 private:
  Authenticator* last_instance_;
};

RemoteDevice CreateTestRemoteDevice() {
  RemoteDevice remote_device = GenerateTestRemoteDevices(1)[0];
  remote_device.user_id = test_user_id;
  return remote_device;
}

}  // namespace

class CryptAuthSecureChannelTest : public testing::Test {
 protected:
  CryptAuthSecureChannelTest()
      : test_device_(CreateTestRemoteDevice()),
        weak_ptr_factory_(this) {}

  void SetUp() override {
    test_authenticator_factory_ = base::MakeUnique<TestAuthenticatorFactory>();
    DeviceToDeviceAuthenticator::Factory::SetInstanceForTesting(
        test_authenticator_factory_.get());

    fake_secure_context_ = nullptr;

    fake_cryptauth_service_ = base::MakeUnique<FakeCryptAuthService>();

    fake_connection_ =
        new FakeConnection(test_device_, /* should_auto_connect */ false);

    EXPECT_FALSE(fake_connection_->observers().size());
    secure_channel_ = base::WrapUnique(new SecureChannel(
        base::WrapUnique(fake_connection_), fake_cryptauth_service_.get()));
    EXPECT_EQ(static_cast<size_t>(1), fake_connection_->observers().size());
    EXPECT_EQ(secure_channel_.get(), fake_connection_->observers()[0]);

    test_observer_ = base::MakeUnique<TestObserver>(secure_channel_.get());
    secure_channel_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    // All state changes should have already been verified. This ensures that
    // no test has missed one.
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>());

    // Same with received messages.
    VerifyReceivedMessages(std::vector<ReceivedMessage>());

    // Same with messages being sent.
    if (secure_channel_)
      VerifyNoMessageBeingSent();
  }

  void VerifyConnectionStateChanges(
      const std::vector<SecureChannelStatusChange>& expected_changes) {
    verified_status_changes_.insert(
        verified_status_changes_.end(),
        expected_changes.begin(),
        expected_changes.end());

    ASSERT_EQ(
        verified_status_changes_.size(),
        test_observer_->connection_status_changes().size());

    for (size_t i = 0; i < verified_status_changes_.size(); i++) {
      EXPECT_EQ(
          verified_status_changes_[i].old_status,
          test_observer_->connection_status_changes()[i].old_status);
      EXPECT_EQ(
          verified_status_changes_[i].new_status,
          test_observer_->connection_status_changes()[i].new_status);
    }
  }

  void VerifyReceivedMessages(
      const std::vector<ReceivedMessage>& expected_messages) {
    verified_received_messages_.insert(
        verified_received_messages_.end(),
        expected_messages.begin(),
        expected_messages.end());

    ASSERT_EQ(
        verified_received_messages_.size(),
        test_observer_->received_messages().size());

    for (size_t i = 0; i < verified_received_messages_.size(); i++) {
      EXPECT_EQ(
          verified_received_messages_[i].feature,
          test_observer_->received_messages()[i].feature);
      EXPECT_EQ(
          verified_received_messages_[i].payload,
          test_observer_->received_messages()[i].payload);
    }
  }

  void FailAuthentication(Authenticator::Result result) {
    ASSERT_NE(result, Authenticator::Result::SUCCESS);

    FakeAuthenticator* authenticator = static_cast<FakeAuthenticator*>(
        test_authenticator_factory_->last_instance());
    authenticator->last_callback().Run(result, nullptr);
  }

  void AuthenticateSuccessfully() {
    FakeAuthenticator* authenticator = static_cast<FakeAuthenticator*>(
        test_authenticator_factory_->last_instance());

    fake_secure_context_ = new FakeSecureContext();
    authenticator->last_callback().Run(
        Authenticator::Result::SUCCESS, base::WrapUnique(fake_secure_context_));
  }

  void ConnectAndAuthenticate() {
    secure_channel_->Initialize();
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
        {
          SecureChannel::Status::DISCONNECTED,
          SecureChannel::Status::CONNECTING
        }
    });

    fake_connection_->CompleteInProgressConnection(/* success */ true);
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
        {
          SecureChannel::Status::CONNECTING,
          SecureChannel::Status::CONNECTED
        },
        {
          SecureChannel::Status::CONNECTED,
          SecureChannel::Status::AUTHENTICATING
        }
    });

    AuthenticateSuccessfully();
    VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
        {
          SecureChannel::Status::AUTHENTICATING,
          SecureChannel::Status::AUTHENTICATED
        }
    });
  }

  // Starts sending the message and returns the sequence number.
  int StartSendingMessage(const std::string& feature,
                          const std::string& payload) {
    int sequence_number = secure_channel_->SendMessage(feature, payload);
    VerifyMessageBeingSent(feature, payload);
    return sequence_number;
  }

  void FinishSendingMessage(int sequence_number, bool success) {
    std::vector<int> sent_sequence_numbers_before_send =
        test_observer_->sent_sequence_numbers();

    fake_connection_->FinishSendingMessageWithSuccess(success);

    if (success) {
      std::vector<int> sent_sequence_numbers_after_send =
          test_observer_->sent_sequence_numbers();
      EXPECT_EQ(sent_sequence_numbers_before_send.size() + 1u,
                sent_sequence_numbers_after_send.size());
      EXPECT_EQ(sequence_number, sent_sequence_numbers_after_send.back());
    }
  }

  void StartAndFinishSendingMessage(
      const std::string& feature, const std::string& payload, bool success) {
    int sequence_number = StartSendingMessage(feature, payload);
    FinishSendingMessage(sequence_number, success);
  }

  void VerifyNoMessageBeingSent() {
    EXPECT_FALSE(fake_connection_->current_message());
  }

  void VerifyMessageBeingSent(
      const std::string& feature, const std::string& payload) {
    WireMessage* message_being_sent = fake_connection_->current_message();
    // Note that despite the fact that |Encode()| has an asynchronous interface,
    // the implementation will call |VerifyWireMessageContents()| synchronously.
    fake_secure_context_->Encode(
        payload,
        base::Bind(&CryptAuthSecureChannelTest::VerifyWireMessageContents,
                   weak_ptr_factory_.GetWeakPtr(),
                   message_being_sent,
                   feature));
  }

  void VerifyWireMessageContents(
      WireMessage* wire_message,
      const std::string& expected_feature,
      const std::string& expected_payload) {
    EXPECT_EQ(expected_feature, wire_message->feature());
    EXPECT_EQ(expected_payload, wire_message->payload());
  }

  // Owned by secure_channel_.
  FakeConnection* fake_connection_;

  std::unique_ptr<FakeCryptAuthService> fake_cryptauth_service_;

  // Owned by secure_channel_ once authentication has completed successfully.
  FakeSecureContext* fake_secure_context_;

  std::vector<SecureChannelStatusChange> verified_status_changes_;

  std::vector<ReceivedMessage> verified_received_messages_;

  std::unique_ptr<SecureChannel> secure_channel_;

  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<TestAuthenticatorFactory> test_authenticator_factory_;

  const RemoteDevice test_device_;

  base::WeakPtrFactory<CryptAuthSecureChannelTest> weak_ptr_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthSecureChannelTest);
};

TEST_F(CryptAuthSecureChannelTest, ConnectionAttemptFails) {
  secure_channel_->Initialize();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::DISCONNECTED,
        SecureChannel::Status::CONNECTING
      }
  });

  fake_connection_->CompleteInProgressConnection(/* success */ false);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::CONNECTING,
        SecureChannel::Status::DISCONNECTED
      }
  });
}

TEST_F(CryptAuthSecureChannelTest, DisconnectBeforeAuthentication) {
  secure_channel_->Initialize();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::DISCONNECTED,
        SecureChannel::Status::CONNECTING
      }
  });

  fake_connection_->Disconnect();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::CONNECTING,
        SecureChannel::Status::DISCONNECTED
      }
  });
}

TEST_F(CryptAuthSecureChannelTest, AuthenticationFails_Disconnect) {
  secure_channel_->Initialize();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::DISCONNECTED,
        SecureChannel::Status::CONNECTING
      }
  });

  fake_connection_->CompleteInProgressConnection(/* success */ true);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::CONNECTING,
        SecureChannel::Status::CONNECTED
      },
      {
        SecureChannel::Status::CONNECTED,
        SecureChannel::Status::AUTHENTICATING
      }
  });

  FailAuthentication(Authenticator::Result::DISCONNECTED);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATING,
        SecureChannel::Status::DISCONNECTED
      }
  });
}

TEST_F(CryptAuthSecureChannelTest, AuthenticationFails_Failure) {
  secure_channel_->Initialize();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::DISCONNECTED,
        SecureChannel::Status::CONNECTING
      }
  });

  fake_connection_->CompleteInProgressConnection(/* success */ true);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::CONNECTING,
        SecureChannel::Status::CONNECTED
      },
      {
        SecureChannel::Status::CONNECTED,
        SecureChannel::Status::AUTHENTICATING
      }
  });

  FailAuthentication(Authenticator::Result::FAILURE);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATING,
        SecureChannel::Status::DISCONNECTED
      }
  });
}

TEST_F(CryptAuthSecureChannelTest, SendMessage_DisconnectWhileSending) {
  ConnectAndAuthenticate();
  int sequence_number = StartSendingMessage("feature", "payload");

  fake_connection_->Disconnect();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATED,
        SecureChannel::Status::DISCONNECTED
      }
  });

  FinishSendingMessage(sequence_number, false);
  // No further state change should have occurred.
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>());
}

TEST_F(
    CryptAuthSecureChannelTest,
    SendMessage_DisconnectWhileSending_ThenSendCompletedOccurs) {
  ConnectAndAuthenticate();
  StartSendingMessage("feature", "payload");

  fake_connection_->Disconnect();
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATED,
        SecureChannel::Status::DISCONNECTED
      }
  });

  // If, due to a race condition, a disconnection occurs and |SendCompleted()|
  // is called in the success case, nothing should occur.
  fake_connection_->FinishSendingMessageWithSuccess(true);

  // No further state change should have occurred.
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange>());
}

TEST_F(CryptAuthSecureChannelTest, SendMessage_Failure) {
  ConnectAndAuthenticate();
  StartAndFinishSendingMessage("feature", "payload", /* success */ false);
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATED,
        SecureChannel::Status::DISCONNECTED
      }
  });
}

TEST_F(CryptAuthSecureChannelTest, SendMessage_Success) {
  ConnectAndAuthenticate();
  StartAndFinishSendingMessage("feature", "payload", /* success */ true);
}

TEST_F(CryptAuthSecureChannelTest, SendMessage_MultipleMessages_Success) {
  ConnectAndAuthenticate();

  // Send a second message before the first has completed.
  int sequence_number1 = secure_channel_->SendMessage("feature1", "payload1");
  int sequence_number2 = secure_channel_->SendMessage("feature2", "payload2");

  // The first message should still be sending.
  VerifyMessageBeingSent("feature1", "payload1");

  // Send the first message.
  FinishSendingMessage(sequence_number1, true);

  // Now, the second message should be sending.
  VerifyMessageBeingSent("feature2", "payload2");
  FinishSendingMessage(sequence_number2, true);
}

TEST_F(CryptAuthSecureChannelTest, SendMessage_MultipleMessages_FirstFails) {
  ConnectAndAuthenticate();

  // Send a second message before the first has completed.
  int sequence_number1 = secure_channel_->SendMessage("feature1", "payload1");
  secure_channel_->SendMessage("feature2", "payload2");

  // The first message should still be sending.
  VerifyMessageBeingSent("feature1", "payload1");

  // Fail sending the first message.
  FinishSendingMessage(sequence_number1, false);

  // The connection should have become disconnected.
  VerifyConnectionStateChanges(std::vector<SecureChannelStatusChange> {
      {
        SecureChannel::Status::AUTHENTICATED,
        SecureChannel::Status::DISCONNECTED
      }
  });

  // The first message failed, so no other ones should be tried afterward.
  VerifyNoMessageBeingSent();
}

TEST_F(CryptAuthSecureChannelTest, ReceiveMessage) {
  ConnectAndAuthenticate();

  // Note: FakeSecureContext's Encode() function simply adds ", but encoded" to
  // the end of the message.
  fake_connection_->ReceiveMessage("feature", "payload, but encoded");
  VerifyReceivedMessages(std::vector<ReceivedMessage> {
      {"feature", "payload"}
  });
}

TEST_F(CryptAuthSecureChannelTest, SendAndReceiveMessages) {
  ConnectAndAuthenticate();

  StartAndFinishSendingMessage("feature", "request1", /* success */ true);

  // Note: FakeSecureContext's Encode() function simply adds ", but encoded" to
  // the end of the message.
  fake_connection_->ReceiveMessage("feature", "response1, but encoded");
  VerifyReceivedMessages(std::vector<ReceivedMessage> {
      {"feature", "response1"}
  });

  StartAndFinishSendingMessage("feature", "request2", /* success */ true);

  fake_connection_->ReceiveMessage("feature", "response2, but encoded");
  VerifyReceivedMessages(std::vector<ReceivedMessage> {
      {"feature", "response2"}
  });
}

TEST_F(CryptAuthSecureChannelTest, ObserverDeletesChannel) {
  // Add a special Observer which deletes |secure_channel_| once it receives an
  // OnMessageSent() call.
  std::unique_ptr<DeletingObserver> deleting_observer =
      base::WrapUnique(new DeletingObserver(&secure_channel_));
  secure_channel_->AddObserver(deleting_observer.get());

  ConnectAndAuthenticate();

  // Send a message successfully; this triggers an OnMessageSent() call which
  // deletes the channel. Note that this would have caused a crash before the
  // fix for crbug.com/751884.
  StartAndFinishSendingMessage("feature", "request1", /* success */ true);
  EXPECT_FALSE(secure_channel_);
}

}  // namespace cryptauth
