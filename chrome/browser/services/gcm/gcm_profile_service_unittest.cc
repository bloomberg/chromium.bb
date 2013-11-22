// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

const char kTestingUsername[] = "user@example.com";
const char kTestingAppId[] = "test1";
const char kTestingSha1Cert[] = "testing_cert1";
const char kUserId[] = "user2";

class GCMProfileServiceTest;

class GCMEventRouterMock : public GCMEventRouter {
 public:
  enum Event {
    NO_EVENT,
    MESSAGE_EVENT,
    MESSAGES_DELETED_EVENT,
    SEND_ERROR_EVENT
  };

  explicit GCMEventRouterMock(GCMProfileServiceTest* test);
  virtual ~GCMEventRouterMock();

  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnSendError(const std::string& app_id,
                           const std::string& message_id,
                           GCMClient::Result result) OVERRIDE;

  Event received_event() const { return received_event_; }
  const std::string& app_id() const { return app_id_; }
  const GCMClient::IncomingMessage incoming_message() const {
    return incoming_message_;
  }
  const std::string& send_message_id() const { return send_error_message_id_; }
  GCMClient::Result send_result() const { return send_error_result_; }

 private:
  GCMProfileServiceTest* test_;
  Event received_event_;
  std::string app_id_;
  GCMClient::IncomingMessage incoming_message_;
  std::string send_error_message_id_;
  GCMClient::Result send_error_result_;
};

class GCMProfileServiceTest : public testing::Test,
                              public GCMProfileService::TestingDelegate {
 public:
  GCMProfileServiceTest() {
  }

  virtual ~GCMProfileServiceTest() {
  }

  // Overridden from test::Test:
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    thread_bundle_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::REAL_IO_THREAD));

    GCMProfileService::enable_gcm_for_testing_ = true;

    // Mock a signed-in user.
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile());
    signin_manager->SetAuthenticatedUsername(kTestingUsername);

    // Mock a GCMClient.
    gcm_client_mock_.reset(new GCMClientMock());
    GCMClient::SetForTesting(gcm_client_mock_.get());

    // Mock a GCMEventRouter.
    gcm_event_router_mock_.reset(new GCMEventRouterMock(this));

    // Pass delegate to observe some events.
    GetGCMProfileService()->set_testing_delegate(this);

    // Wait till the asynchronous check-in is done.
    WaitForCompleted();
  }

  virtual void TearDown() OVERRIDE {
    GCMClient::SetForTesting(NULL);
  }

  // Overridden from GCMProfileService::TestingDelegate:
  virtual GCMEventRouter* GetEventRouter() const OVERRIDE {
    return gcm_event_router_mock_.get();
  }

  virtual void CheckInFinished(const GCMClient::CheckInInfo& checkin_info,
                               GCMClient::Result result) OVERRIDE {
    checkin_info_ = checkin_info;
    SignalCompleted();
  }

  // Waits until the asynchrnous operation finishes.
  void WaitForCompleted() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  // Signals that the asynchrnous operation finishes.
  void SignalCompleted() {
    run_loop_->Quit();
  }

  Profile* profile() const { return profile_.get(); }

  GCMProfileService* GetGCMProfileService() const {
    return GCMProfileServiceFactory::GetForProfile(profile());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<GCMClientMock> gcm_client_mock_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<GCMEventRouterMock> gcm_event_router_mock_;
  GCMClient::CheckInInfo checkin_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTest);
};

GCMEventRouterMock::GCMEventRouterMock(GCMProfileServiceTest* test)
    : test_(test),
      received_event_(NO_EVENT),
      send_error_result_(GCMClient::SUCCESS) {
}

GCMEventRouterMock::~GCMEventRouterMock() {
}

void GCMEventRouterMock::OnMessage(const std::string& app_id,
                                   const GCMClient::IncomingMessage& message) {
  received_event_ = MESSAGE_EVENT;
  app_id_ = app_id;
  incoming_message_ = message;
  test_->SignalCompleted();
}

void GCMEventRouterMock::OnMessagesDeleted(const std::string& app_id) {
  received_event_ = MESSAGES_DELETED_EVENT;
  app_id_ = app_id;
  test_->SignalCompleted();
}

void GCMEventRouterMock::OnSendError(const std::string& app_id,
                                     const std::string& message_id,
                                     GCMClient::Result result) {
  received_event_ = SEND_ERROR_EVENT;
  app_id_ = app_id;
  send_error_message_id_ = message_id;
  send_error_result_ = result;
  test_->SignalCompleted();
}

TEST_F(GCMProfileServiceTest, CheckIn) {
  EXPECT_TRUE(checkin_info_.IsValid());

  GCMClient::CheckInInfo expected_checkin_info =
      gcm_client_mock_->GetCheckInInfoFromUsername(kTestingUsername);
  EXPECT_EQ(expected_checkin_info.android_id, checkin_info_.android_id);
  EXPECT_EQ(expected_checkin_info.secret, checkin_info_.secret);
}

class GCMProfileServiceRegisterTest : public GCMProfileServiceTest {
 public:
  GCMProfileServiceRegisterTest() : result_(GCMClient::SUCCESS) {
  }

  virtual ~GCMProfileServiceRegisterTest() {
  }

  void Register(const std::vector<std::string>& sender_ids) {
    GetGCMProfileService()->Register(
        kTestingAppId,
        sender_ids,
        kTestingSha1Cert,
        base::Bind(&GCMProfileServiceRegisterTest::RegisterCompleted,
                   base::Unretained(this)));
  }

  void RegisterCompleted(const std::string& registration_id,
                         GCMClient::Result result) {
    registration_id_ = registration_id;
    result_ = result;
    SignalCompleted();
  }

 protected:
  std::string registration_id_;
  GCMClient::Result result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceRegisterTest);
};

TEST_F(GCMProfileServiceRegisterTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(sender_ids);
  std::string expected_registration_id =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  WaitForCompleted();
  EXPECT_FALSE(registration_id_.empty());
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceRegisterTest, DoubleRegister) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(sender_ids);
  std::string expected_registration_id =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  // Calling regsiter 2nd time without waiting 1st one to finish will fail
  // immediately.
  sender_ids.push_back("sender2");
  Register(sender_ids);
  EXPECT_TRUE(registration_id_.empty());
  EXPECT_NE(GCMClient::SUCCESS, result_);

  // The 1st register is still doing fine.
  WaitForCompleted();
  EXPECT_FALSE(registration_id_.empty());
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceRegisterTest, RegisterError) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1@error");
  Register(sender_ids);

  WaitForCompleted();
  EXPECT_TRUE(registration_id_.empty());
  EXPECT_NE(GCMClient::SUCCESS, result_);
}

class GCMProfileServiceSendTest : public GCMProfileServiceTest {
 public:
  GCMProfileServiceSendTest() : result_(GCMClient::SUCCESS) {
  }

  virtual ~GCMProfileServiceSendTest() {
  }

  void Send(const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message) {
    GetGCMProfileService()->Send(
        kTestingAppId,
        receiver_id,
        message,
        base::Bind(&GCMProfileServiceSendTest::SendCompleted,
                   base::Unretained(this)));
  }

  void SendCompleted(const std::string& message_id, GCMClient::Result result) {
    message_id_ = message_id;
    result_ = result;
    SignalCompleted();
  }

 protected:
  std::string message_id_;
  GCMClient::Result result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceSendTest);
};

TEST_F(GCMProfileServiceSendTest, Send) {
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kUserId, message);

  // Wait for the send callback is called.
  WaitForCompleted();
  EXPECT_EQ(message_id_, message.id);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceSendTest, SendError) {
  GCMClient::OutgoingMessage message;
  // Embedding error in id will tell the mock to simulate the send error.
  message.id = "1@error";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kUserId, message);

  // Wait for the send callback is called.
  WaitForCompleted();
  EXPECT_EQ(message_id_, message.id);
  EXPECT_EQ(GCMClient::SUCCESS, result_);

  // Wait for the send error.
  WaitForCompleted();
  EXPECT_EQ(GCMEventRouterMock::SEND_ERROR_EVENT,
            gcm_event_router_mock_->received_event());
  EXPECT_EQ(kTestingAppId, gcm_event_router_mock_->app_id());
  EXPECT_EQ(message_id_, gcm_event_router_mock_->send_message_id());
  EXPECT_NE(GCMClient::SUCCESS, gcm_event_router_mock_->send_result());
}

TEST_F(GCMProfileServiceTest, MessageReceived) {
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  gcm_client_mock_->ReceiveMessage(kTestingUsername, kTestingAppId, message);
  WaitForCompleted();
  EXPECT_EQ(GCMEventRouterMock::MESSAGE_EVENT,
            gcm_event_router_mock_->received_event());
  EXPECT_EQ(kTestingAppId, gcm_event_router_mock_->app_id());
  ASSERT_EQ(message.data.size(),
            gcm_event_router_mock_->incoming_message().data.size());
  EXPECT_EQ(
      message.data.find("key1")->second,
      gcm_event_router_mock_->incoming_message().data.find("key1")->second);
  EXPECT_EQ(
      message.data.find("key2")->second,
      gcm_event_router_mock_->incoming_message().data.find("key2")->second);
}

TEST_F(GCMProfileServiceTest, MessagesDeleted) {
  gcm_client_mock_->DeleteMessages(kTestingUsername, kTestingAppId);
  WaitForCompleted();
  EXPECT_EQ(GCMEventRouterMock::MESSAGES_DELETED_EVENT,
            gcm_event_router_mock_->received_event());
  EXPECT_EQ(kTestingAppId, gcm_event_router_mock_->app_id());
}

}  // namespace gcm
