// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_network_handler.h"

#include <memory>
#include <string>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "google_apis/gcm/engine/account_mapping.h"
#include "testing/gmock/include/gmock/gmock.h"

using gcm::InstanceIDHandler;
using instance_id::InstanceID;
using instance_id::InstanceIDDriver;
using testing::_;
using testing::StrictMock;

namespace syncer {

namespace {

const char kInvalidationsAppId[] = "com.google.chrome.fcm.invalidations";
using DataCallback = base::RepeatingCallback<void(const std::string& message)>;

class MockInstanceID : public InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;

  MOCK_METHOD1(GetID, void(const GetIDCallback& callback));
  MOCK_METHOD1(GetCreationTime, void(const GetCreationTimeCallback& callback));
  MOCK_METHOD4(GetToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::map<std::string, std::string>& options,
                    const GetTokenCallback& callback));
  MOCK_METHOD4(ValidateToken,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const std::string& token,
                    const ValidateTokenCallback& callback));

 protected:
  MOCK_METHOD3(DeleteTokenImpl,
               void(const std::string& authorized_entity,
                    const std::string& scope,
                    const DeleteTokenCallback& callback));
  MOCK_METHOD1(DeleteIDImpl, void(const DeleteIDCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceID);
};

class MockGCMDriver : public gcm::GCMDriver {
 public:
  MockGCMDriver()
      : GCMDriver(/*store_path=*/base::FilePath(),
                  /*blocking_task_runner=*/nullptr) {}
  ~MockGCMDriver() override = default;

  MOCK_METHOD4(ValidateRegistration,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids,
                    const std::string& registration_id,
                    const ValidateRegistrationCallback& callback));
  MOCK_METHOD0(OnSignedIn, void());
  MOCK_METHOD0(OnSignedOut, void());
  MOCK_METHOD1(AddConnectionObserver,
               void(gcm::GCMConnectionObserver* observer));
  MOCK_METHOD1(RemoveConnectionObserver,
               void(gcm::GCMConnectionObserver* observer));
  MOCK_METHOD0(Enable, void());
  MOCK_METHOD0(Disable, void());
  MOCK_CONST_METHOD0(GetGCMClientForTesting, gcm::GCMClient*());
  MOCK_CONST_METHOD0(IsStarted, bool());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD2(GetGCMStatistics,
               void(const GetGCMStatisticsCallback& callback,
                    ClearActivityLogs clear_logs));
  MOCK_METHOD2(SetGCMRecording,
               void(const GetGCMStatisticsCallback& callback, bool recording));
  MOCK_METHOD1(SetAccountTokens,
               void(const std::vector<gcm::GCMClient::AccountTokenInfo>&
                        account_tokens));
  MOCK_METHOD1(UpdateAccountMapping,
               void(const gcm::AccountMapping& account_mapping));
  MOCK_METHOD1(RemoveAccountMapping, void(const std::string& account_id));
  MOCK_METHOD0(GetLastTokenFetchTime, base::Time());
  MOCK_METHOD1(SetLastTokenFetchTime, void(const base::Time& time));
  MOCK_METHOD1(WakeFromSuspendForHeartbeat, void(bool wake));
  MOCK_METHOD0(GetInstanceIDHandlerInternal, InstanceIDHandler*());
  MOCK_METHOD2(AddHeartbeatInterval,
               void(const std::string& scope, int interval_ms));
  MOCK_METHOD1(RemoveHeartbeatInterval, void(const std::string& scope));

 protected:
  MOCK_METHOD1(EnsureStarted,
               gcm::GCMClient::Result(gcm::GCMClient::StartMode start_mode));
  MOCK_METHOD2(RegisterImpl,
               void(const std::string& app_id,
                    const std::vector<std::string>& sender_ids));
  MOCK_METHOD1(UnregisterImpl, void(const std::string& app_id));
  MOCK_METHOD3(SendImpl,
               void(const std::string& app_id,
                    const std::string& receiver_id,
                    const gcm::OutgoingMessage& message));
  MOCK_METHOD2(RecordDecryptionFailure,
               void(const std::string& app_id,
                    gcm::GCMDecryptionResult result));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGCMDriver);
};

class MockInstanceIDDriver : public InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr){};
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD1(GetInstanceID, InstanceID*(const std::string& app_id));
  MOCK_METHOD1(RemoveInstanceID, void(const std::string& app_id));
  MOCK_CONST_METHOD1(ExistsInstanceID, bool(const std::string& app_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInstanceIDDriver);
};

class MockOnDataCallback {
 public:
  // Workaround for gMock's lack of support for movable-only arguments.
  void WrappedRun(const std::string& token) { Run(token); }

  DataCallback Get() {
    return base::BindRepeating(&MockOnDataCallback::WrappedRun,
                               base::Unretained(this));
  }

  MOCK_METHOD1(Run, void(const std::string&));
};

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  std::get<k>(args).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  std::get<k>(args).Run(p0, p1);
}

}  // namespace

class FCMNetworkHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    // Our app handler obtains InstanceID through InstanceIDDriver. We mock
    // InstanceIDDriver and return MockInstanceID through it.
    mock_instance_id_driver_ =
        std::make_unique<StrictMock<MockInstanceIDDriver>>();
    mock_instance_id_ = std::make_unique<StrictMock<MockInstanceID>>();
    mock_gcm_driver_ = std::make_unique<StrictMock<MockGCMDriver>>();

    // This is called in FCMNetworkHandler.
    EXPECT_CALL(*mock_instance_id_driver_, GetInstanceID(kInvalidationsAppId))
        .WillRepeatedly(Return(mock_instance_id_.get()));
  }

  std::unique_ptr<FCMNetworkHandler> MakeHandler() {
    return std::make_unique<FCMNetworkHandler>(mock_gcm_driver_.get(),
                                               mock_instance_id_driver_.get());
  }

  StrictMock<MockInstanceID>* mock_instance_id() {
    return mock_instance_id_.get();
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<StrictMock<MockGCMDriver>> mock_gcm_driver_;
  std::unique_ptr<StrictMock<MockInstanceIDDriver>> mock_instance_id_driver_;
  std::unique_ptr<StrictMock<MockInstanceID>> mock_instance_id_;
};

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceRecieved) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnDataCallback mock_on_token_callback;
  handler->SetTokenReceiver(mock_on_token_callback.Get());

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken(_, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<3>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->StartListening();
}

TEST_F(FCMNetworkHandlerTest, ShouldPassTheTokenOnceSubscribed) {
  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();

  MockOnDataCallback mock_on_token_callback;

  // Check that the handler gets the token through GetToken.
  EXPECT_CALL(*mock_instance_id(), GetToken(_, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<3>("token", InstanceID::Result::SUCCESS));
  EXPECT_CALL(mock_on_token_callback, Run(_)).Times(0);
  handler->StartListening();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(mock_on_token_callback, Run("token")).Times(1);
  handler->SetTokenReceiver(mock_on_token_callback.Get());
}

TEST_F(FCMNetworkHandlerTest, ShouldNotInvokeMessageCallbackOnEmptyMessage) {
  MockOnDataCallback mock_on_message_callback;

  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
  EXPECT_CALL(mock_on_message_callback, Run(_)).Times(0);
  handler->SetMessageReceiver(mock_on_message_callback.Get());
  EXPECT_CALL(*mock_instance_id(), GetToken(_, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<3>("token", InstanceID::Result::SUCCESS));

  handler->StartListening();
  handler->OnMessage(kInvalidationsAppId, gcm::IncomingMessage());
}

TEST_F(FCMNetworkHandlerTest, ShouldInvokeMessageCallbackOnValidMessage) {
  MockOnDataCallback mock_on_message_callback;
  gcm::IncomingMessage message;
  message.data["data"] = "test";

  std::unique_ptr<FCMNetworkHandler> handler = MakeHandler();
  EXPECT_CALL(*mock_instance_id(), GetToken(_, _, _, _))
      .WillOnce(
          InvokeCallbackArgument<3>("token", InstanceID::Result::SUCCESS));
  handler->StartListening();
  EXPECT_CALL(mock_on_message_callback, Run("test")).Times(0);
  handler->SetMessageReceiver(mock_on_message_callback.Get());
  handler->OnMessage(kInvalidationsAppId, gcm::IncomingMessage());
}

}  // namespace syncer
