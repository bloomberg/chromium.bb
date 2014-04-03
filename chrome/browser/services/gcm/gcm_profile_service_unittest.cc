// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/services/gcm/gcm_app_handler.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_test_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestingUsername[] = "user1@example.com";
const char kTestingUsername2[] = "user2@example.com";
const char kTestingUsername3[] = "user3@example.com";
const char kTestingAppId[] = "TestApp1";
const char kTestingAppId2[] = "TestApp2";
const char kUserId[] = "user1";
const char kUserId2[] = "user2";

std::vector<std::string> ToSenderList(const std::string& sender_ids) {
  std::vector<std::string> senders;
  Tokenize(sender_ids, ",", &senders);
  return senders;
}

}  // namespace

class FakeGCMAppHandler : public GCMAppHandler {
 public:
  enum Event {
    NO_EVENT,
    MESSAGE_EVENT,
    MESSAGES_DELETED_EVENT,
    SEND_ERROR_EVENT
  };

  explicit FakeGCMAppHandler(Waiter* waiter)
      : waiter_(waiter),
        received_event_(NO_EVENT) {
  }

  virtual ~FakeGCMAppHandler() {
  }

  virtual void ShutdownHandler() OVERRIDE {
  }

  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) OVERRIDE {
    clear_results();
    received_event_ = MESSAGE_EVENT;
    app_id_ = app_id;
    message_ = message;
    waiter_->SignalCompleted();
  }

  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE {
    clear_results();
    received_event_ = MESSAGES_DELETED_EVENT;
    app_id_ = app_id;
    waiter_->SignalCompleted();
  }

  virtual void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE {
    clear_results();
    received_event_ = SEND_ERROR_EVENT;
    app_id_ = app_id;
    send_error_details_ = send_error_details;
    waiter_->SignalCompleted();
  }

  Event received_event() const { return received_event_; }
  const std::string& app_id() const { return app_id_; }
  const GCMClient::IncomingMessage& message() const { return message_; }
  const GCMClient::SendErrorDetails& send_error_details() const {
    return send_error_details_;
  }
  const std::string& send_error_message_id() const {
    return send_error_details_.message_id;
  }
  GCMClient::Result send_error_result() const {
    return send_error_details_.result;
  }
  const GCMClient::MessageData& send_error_data() const {
    return send_error_details_.additional_data;
  }

  void clear_results() {
    received_event_ = NO_EVENT;
    app_id_.clear();
    message_.data.clear();
    send_error_details_ = GCMClient::SendErrorDetails();
  }

 private:
  Waiter* waiter_;
  Event received_event_;
  std::string app_id_;
  GCMClient::IncomingMessage message_;
  GCMClient::SendErrorDetails send_error_details_;
};

class GCMProfileServiceTestConsumer {
 public:
  static KeyedService* BuildGCMProfileService(
      content::BrowserContext* context) {
    return new GCMProfileService(static_cast<Profile*>(context));
  }

  explicit GCMProfileServiceTestConsumer(Waiter* waiter)
      : waiter_(waiter),
        signin_manager_(NULL),
        gcm_client_loading_delay_(GCMClientMock::NO_DELAY_LOADING),
        registration_result_(GCMClient::UNKNOWN_ERROR),
        unregistration_result_(GCMClient::UNKNOWN_ERROR),
        send_result_(GCMClient::UNKNOWN_ERROR) {
    // Create a new profile.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManager::Build);
    profile_ = builder.Build();
    signin_manager_ = static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetInstance()->GetForProfile(profile_.get()));

    // Enable GCM such that tests could be run on all channels.
    profile()->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);
  }

  virtual ~GCMProfileServiceTestConsumer() {
    GetGCMProfileService()->RemoveAppHandler(kTestingAppId);
    GetGCMProfileService()->RemoveAppHandler(kTestingAppId2);
  }

  void CreateGCMProfileServiceInstance() {
    GCMProfileService* gcm_profile_service = static_cast<GCMProfileService*>(
        GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &GCMProfileServiceTestConsumer::BuildGCMProfileService));
    scoped_ptr<GCMClientFactory> gcm_client_factory(
        new FakeGCMClientFactory(gcm_client_loading_delay_));
    gcm_profile_service->Initialize(gcm_client_factory.Pass());

    gcm_app_handler_.reset(new FakeGCMAppHandler(waiter_));
    gcm_profile_service->AddAppHandler(kTestingAppId, gcm_app_handler());
    gcm_profile_service->AddAppHandler(kTestingAppId2, gcm_app_handler());

    waiter_->PumpIOLoop();
  }

  void SignIn(const std::string& username) {
    signin_manager_->SignIn(username);
    waiter_->PumpIOLoop();
    waiter_->PumpUILoop();
  }

  void SignOut() {
    signin_manager_->SignOut();
    waiter_->PumpIOLoop();
    waiter_->PumpUILoop();
  }

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids) {
    GetGCMProfileService()->Register(
        app_id,
        sender_ids,
        base::Bind(&GCMProfileServiceTestConsumer::RegisterCompleted,
                   base::Unretained(this)));
  }

  void RegisterCompleted(const std::string& registration_id,
                         GCMClient::Result result) {
    registration_id_ = registration_id;
    registration_result_ = result;
    waiter_->SignalCompleted();
  }

  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message) {
    GetGCMProfileService()->Send(
        app_id,
        receiver_id,
        message,
        base::Bind(&GCMProfileServiceTestConsumer::SendCompleted,
                   base::Unretained(this)));
  }

  void SendCompleted(const std::string& message_id, GCMClient::Result result) {
    send_message_id_ = message_id;
    send_result_ = result;
    waiter_->SignalCompleted();
  }

  void Unregister(const std::string& app_id) {
    GetGCMProfileService()->Unregister(
        app_id,
        base::Bind(&GCMProfileServiceTestConsumer::UnregisterCompleted,
                   base::Unretained(this)));
  }

  void UnregisterCompleted(GCMClient::Result result) {
    unregistration_result_ = result;
    waiter_->SignalCompleted();
  }

  GCMProfileService* GetGCMProfileService() const {
    return GCMProfileServiceFactory::GetForProfile(profile());
  }

  GCMClientMock* GetGCMClient() const {
    return static_cast<GCMClientMock*>(
        GetGCMProfileService()->GetGCMClientForTesting());
  }

  const std::string& GetUsername() const {
    return GetGCMProfileService()->username_;
  }

  bool IsGCMClientReady() const {
    return GetGCMProfileService()->gcm_client_ready_;
  }

  bool HasAppHandlers() const {
    return !GetGCMProfileService()->app_handlers_.empty();
  }

  Profile* profile() const { return profile_.get(); }
  FakeGCMAppHandler* gcm_app_handler() const {
    return gcm_app_handler_.get();
  }

  void set_gcm_client_loading_delay(GCMClientMock::LoadingDelay delay) {
    gcm_client_loading_delay_ = delay;
  }

  const std::string& registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }
  const std::string& send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }

  void clear_registration_result() {
    registration_id_.clear();
    registration_result_ = GCMClient::UNKNOWN_ERROR;
  }
  void clear_unregistration_result() {
    unregistration_result_ = GCMClient::UNKNOWN_ERROR;
  }
  void clear_send_result() {
    send_message_id_.clear();
    send_result_ = GCMClient::UNKNOWN_ERROR;
  }

 private:
  Waiter* waiter_;  // Not owned.
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManager* signin_manager_;  // Not owned.
  scoped_ptr<FakeGCMAppHandler> gcm_app_handler_;

  GCMClientMock::LoadingDelay gcm_client_loading_delay_;

  std::string registration_id_;
  GCMClient::Result registration_result_;

  GCMClient::Result unregistration_result_;

  std::string send_message_id_;
  GCMClient::Result send_result_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTestConsumer);
};

class GCMProfileServiceTest : public testing::Test {
 public:
  GCMProfileServiceTest() {
  }

  virtual ~GCMProfileServiceTest() {
  }

  // Overridden from test::Test:
  virtual void SetUp() OVERRIDE {
    // Make BrowserThread work in unittest.
    thread_bundle_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::REAL_IO_THREAD));

    // Create a main profile consumer.
    consumer_.reset(new GCMProfileServiceTestConsumer(&waiter_));
  }

  virtual void TearDown() OVERRIDE {
    consumer_.reset();
    PumpUILoop();
  }

  void WaitUntilCompleted() {
    waiter_.WaitUntilCompleted();
  }

  void SignalCompleted() {
    waiter_.SignalCompleted();
  }

  void PumpUILoop() {
    waiter_.PumpUILoop();
  }

  void PumpIOLoop() {
    waiter_.PumpIOLoop();
  }

  Profile* profile() const { return consumer_->profile(); }
  GCMProfileServiceTestConsumer* consumer() const { return consumer_.get(); }

 protected:
  Waiter waiter_;

 private:
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<GCMProfileServiceTestConsumer> consumer_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTest);
};

TEST_F(GCMProfileServiceTest, Incognito) {
  EXPECT_TRUE(GCMProfileServiceFactory::GetForProfile(profile()));

  // Create an incognito profile.
  TestingProfile::Builder incognito_profile_builder;
  incognito_profile_builder.SetIncognito();
  scoped_ptr<TestingProfile> incognito_profile =
      incognito_profile_builder.Build();
  incognito_profile->SetOriginalProfile(profile());

  EXPECT_FALSE(GCMProfileServiceFactory::GetForProfile(
      incognito_profile.get()));
}

TEST_F(GCMProfileServiceTest, CreateGCMProfileServiceBeforeProfileSignIn) {
  // Create GCMProfileService first.
  consumer()->CreateGCMProfileServiceInstance();
  EXPECT_TRUE(consumer()->GetUsername().empty());

  // Sign in to a profile. This will kick off the check-in.
  consumer()->SignIn(kTestingUsername);
  EXPECT_FALSE(consumer()->GetUsername().empty());
}

TEST_F(GCMProfileServiceTest, CreateGCMProfileServiceAfterProfileSignIn) {
  // Sign in to a profile. This will not initiate the check-in.
  consumer()->SignIn(kTestingUsername);

  // Create GCMProfileService after sign-in.
  consumer()->CreateGCMProfileServiceInstance();
  EXPECT_FALSE(consumer()->GetUsername().empty());
}

TEST_F(GCMProfileServiceTest, Shutdown) {
  consumer()->CreateGCMProfileServiceInstance();
  EXPECT_TRUE(consumer()->HasAppHandlers());

  consumer()->GetGCMProfileService()->Shutdown();
  EXPECT_FALSE(consumer()->HasAppHandlers());
}

TEST_F(GCMProfileServiceTest, SignInAndSignOutUnderPositiveChannelSignal) {
  // Positive channel signal is provided in SetUp.
  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should be loaded.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  consumer()->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, consumer()->GetGCMClient()->status());
}

TEST_F(GCMProfileServiceTest, SignInAndSignOutUnderNegativeChannelSignal) {
  // Negative channel signal will prevent GCMClient from checking in when the
  // profile is signed in.
  profile()->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, false);

  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should not be loaded.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, consumer()->GetGCMClient()->status());

  consumer()->SignOut();

  // Check-out should still be performed.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, consumer()->GetGCMClient()->status());
}

TEST_F(GCMProfileServiceTest, SignOutAndThenSignIn) {
  // Positive channel signal is provided in SetUp.
  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should be loaded.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  consumer()->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, consumer()->GetGCMClient()->status());

  // Sign-in with a different username.
  consumer()->SignIn(kTestingUsername2);

  // GCMClient should be loaded again.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());
}

TEST_F(GCMProfileServiceTest, StopAndRestartGCM) {
  // Positive channel signal is provided in SetUp.
  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should be loaded.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  // Stops the GCM.
  consumer()->GetGCMProfileService()->Stop();
  PumpIOLoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, consumer()->GetGCMClient()->status());

  // Restarts the GCM.
  consumer()->GetGCMProfileService()->Start();
  PumpIOLoop();

  // GCMClient should be loaded.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  // Stops the GCM.
  consumer()->GetGCMProfileService()->Stop();
  PumpIOLoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, consumer()->GetGCMClient()->status());

  // Signs out.
  consumer()->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, consumer()->GetGCMClient()->status());
}

TEST_F(GCMProfileServiceTest, RegisterWhenNotSignedIn) {
  consumer()->CreateGCMProfileServiceInstance();

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);

  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->registration_result());
}

TEST_F(GCMProfileServiceTest, RegisterUnderNeutralChannelSignal) {
  // Neutral channel signal will prevent GCMClient from checking in when the
  // profile is signed in.
  profile()->GetPrefs()->ClearPref(prefs::kGCMChannelEnabled);

  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should not be checked in.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, consumer()->GetGCMClient()->status());

  // Invoking register will make GCMClient checked in.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  WaitUntilCompleted();

  // GCMClient should be checked in.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  // Registration should succeed.
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);
  EXPECT_EQ(expected_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceTest, SendWhenNotSignedIn) {
  consumer()->CreateGCMProfileServiceInstance();

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  consumer()->Send(kTestingAppId, kUserId, message);

  EXPECT_TRUE(consumer()->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->send_result());
}

TEST_F(GCMProfileServiceTest, SendUnderNeutralChannelSignal) {
  // Neutral channel signal will prevent GCMClient from checking in when the
  // profile is signed in.
  profile()->GetPrefs()->ClearPref(prefs::kGCMChannelEnabled);

  consumer()->CreateGCMProfileServiceInstance();
  consumer()->SignIn(kTestingUsername);

  // GCMClient should not be checked in.
  EXPECT_FALSE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, consumer()->GetGCMClient()->status());

  // Invoking send will make GCMClient checked in.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  consumer()->Send(kTestingAppId, kUserId, message);
  WaitUntilCompleted();

  // GCMClient should be checked in.
  EXPECT_TRUE(consumer()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, consumer()->GetGCMClient()->status());

  // Sending should succeed.
  EXPECT_EQ(consumer()->send_message_id(), message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());
}

// Tests single-profile.
class GCMProfileServiceSingleProfileTest : public GCMProfileServiceTest {
 public:
  GCMProfileServiceSingleProfileTest() {
  }

  virtual ~GCMProfileServiceSingleProfileTest() {
  }

  virtual void SetUp() OVERRIDE {
    GCMProfileServiceTest::SetUp();

    consumer()->CreateGCMProfileServiceInstance();
    consumer()->SignIn(kTestingUsername);
  }
};

TEST_F(GCMProfileServiceSingleProfileTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  WaitUntilCompleted();
  EXPECT_EQ(expected_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, RegisterError) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1@error");
  consumer()->Register(kTestingAppId, sender_ids);

  WaitUntilCompleted();
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_NE(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, RegisterAgainWithSameSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  sender_ids.push_back("sender2");
  consumer()->Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  WaitUntilCompleted();
  EXPECT_EQ(expected_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  consumer()->clear_registration_result();

  // Calling register 2nd time with the same set of sender IDs but different
  // ordering will get back the same registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, another_sender_ids);

  WaitUntilCompleted();
  EXPECT_EQ(expected_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest,
       RegisterAgainWithDifferentSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  WaitUntilCompleted();
  EXPECT_EQ(expected_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  std::string expected_registration_id2 =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  // Calling register 2nd time with the different sender IDs will get back a new
  // registration ID.
  consumer()->Register(kTestingAppId, sender_ids);
  WaitUntilCompleted();
  EXPECT_EQ(expected_registration_id2, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest,
       GCMClientNotReadyBeforeRegistration) {
  // Make GCMClient not ready initially.
  consumer()->set_gcm_client_loading_delay(GCMClientMock::DELAY_LOADING);
  consumer()->CreateGCMProfileServiceInstance();

  // The registration is on hold until GCMClient is ready.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  PumpIOLoop();
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, consumer()->registration_result());

  // Register operation will be invoked after GCMClient becomes ready.
  consumer()->GetGCMClient()->PerformDelayedLoading();
  PumpIOLoop();  // The 1st pump is to wait till the delayed loading is done.
  PumpIOLoop();  // The 2nd pump is to wait till the registration is done.
  EXPECT_FALSE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, RegisterAfterSignOut) {
  // This will trigger check-out.
  consumer()->SignOut();

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);

  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, UnregisterExplicitly) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);

  WaitUntilCompleted();
  EXPECT_FALSE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  consumer()->Unregister(kTestingAppId);

  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->unregistration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest,
       UnregisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  consumer()->Register(kTestingAppId, sender_ids);

  // Test that unregistration fails with async operation pending when there is a
  // registration already in progress.
  consumer()->Unregister(kTestingAppId);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            consumer()->unregistration_result());

  // Complete the registration.
  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  consumer()->Unregister(kTestingAppId);

  // Test that unregistration fails with async operation pending when there is
  // an unregistration already in progress.
  consumer()->Unregister(kTestingAppId);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            consumer()->unregistration_result());
  consumer()->clear_unregistration_result();

  // Complete unregistration.
  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->unregistration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, RegisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  consumer()->Register(kTestingAppId, sender_ids);

  // Test that registration fails with async operation pending when there is a
  // registration already in progress.
  consumer()->Register(kTestingAppId, sender_ids);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            consumer()->registration_result());
  consumer()->clear_registration_result();

  // Complete the registration.
  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  consumer()->Unregister(kTestingAppId);

  // Test that registration fails with async operation pending when there is an
  // unregistration already in progress.
  consumer()->Register(kTestingAppId, sender_ids);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            consumer()->registration_result());

  // Complete the first unregistration expecting success.
  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->unregistration_result());

  // Test that it is ok to register again after unregistration.
  consumer()->Register(kTestingAppId, sender_ids);
  WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}


TEST_F(GCMProfileServiceSingleProfileTest, Send) {
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  consumer()->Send(kTestingAppId, kUserId, message);

  // Wait for the send callback is called.
  WaitUntilCompleted();
  EXPECT_EQ(consumer()->send_message_id(), message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, GCMClientNotReadyBeforeSending) {
  // Make GCMClient not ready initially.
  consumer()->set_gcm_client_loading_delay(GCMClientMock::DELAY_LOADING);
  consumer()->CreateGCMProfileServiceInstance();

  // The sending is on hold until GCMClient is ready.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  consumer()->Send(kTestingAppId, kUserId, message);
  PumpIOLoop();
  EXPECT_TRUE(consumer()->send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, consumer()->send_result());

  // Register operation will be invoked after GCMClient becomes ready.
  consumer()->GetGCMClient()->PerformDelayedLoading();
  PumpIOLoop();
  PumpIOLoop();
  EXPECT_EQ(consumer()->send_message_id(), message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, SendAfterSignOut) {
  // This will trigger check-out.
  consumer()->SignOut();

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  consumer()->Send(kTestingAppId, kUserId, message);

  EXPECT_TRUE(consumer()->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->send_result());
}

TEST_F(GCMProfileServiceSingleProfileTest, SendError) {
  GCMClient::OutgoingMessage message;
  // Embedding error in id will tell the mock to simulate the send error.
  message.id = "1@error";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  consumer()->Send(kTestingAppId, kUserId, message);

  // Wait for the send callback is called.
  WaitUntilCompleted();
  EXPECT_EQ(consumer()->send_message_id(), message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());

  // Wait for the send error.
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
  EXPECT_EQ(consumer()->send_message_id(),
            consumer()->gcm_app_handler()->send_error_message_id());
  EXPECT_NE(GCMClient::SUCCESS,
            consumer()->gcm_app_handler()->send_error_result());
  EXPECT_EQ(message.data, consumer()->gcm_app_handler()->send_error_data());
}

TEST_F(GCMProfileServiceSingleProfileTest, MessageReceived) {
  consumer()->Register(kTestingAppId, ToSenderList("sender"));
  WaitUntilCompleted();
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_app_handler()->message().data);
  EXPECT_TRUE(consumer()->gcm_app_handler()->message().collapse_key.empty());
  EXPECT_EQ(message.sender_id,
            consumer()->gcm_app_handler()->message().sender_id);
}

TEST_F(GCMProfileServiceSingleProfileTest, MessageWithCollapseKeyReceived) {
  consumer()->Register(kTestingAppId, ToSenderList("sender"));
  WaitUntilCompleted();
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.collapse_key = "collapse_key_value";
  message.sender_id = "sender";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_app_handler()->message().data);
  EXPECT_EQ(message.collapse_key,
            consumer()->gcm_app_handler()->message().collapse_key);
}

TEST_F(GCMProfileServiceSingleProfileTest, MessagesDeleted) {
  consumer()->GetGCMClient()->DeleteMessages(kTestingAppId);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
}

// Tests to make sure that GCMProfileService works for multiple profiles
// regardless how GCMClient is created.
class GCMProfileServiceMultiProfileTest : public GCMProfileServiceTest {
 public:
  GCMProfileServiceMultiProfileTest() {
  }

  virtual ~GCMProfileServiceMultiProfileTest() {
  }

  virtual void SetUp() OVERRIDE {
    GCMProfileServiceTest::SetUp();

    // Create a 2nd profile consumer.
    consumer2_.reset(new GCMProfileServiceTestConsumer(&waiter_));

    consumer()->CreateGCMProfileServiceInstance();
    consumer2()->CreateGCMProfileServiceInstance();

    // Initiate check-in for each profile.
    consumer2()->SignIn(kTestingUsername2);
    consumer()->SignIn(kTestingUsername);
  }

  virtual void TearDown() OVERRIDE {
    consumer2_.reset();

    GCMProfileServiceTest::TearDown();
  }

  Profile* profile2() const { return consumer2_->profile(); }
  GCMProfileServiceTestConsumer* consumer2() const { return consumer2_.get(); }

 protected:
  scoped_ptr<GCMProfileServiceTestConsumer> consumer2_;
};

TEST_F(GCMProfileServiceMultiProfileTest, Register) {
  // Register an app.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);

  // Register the same app in a different profile.
  std::vector<std::string> sender_ids2;
  sender_ids2.push_back("foo");
  sender_ids2.push_back("bar");
  consumer2()->Register(kTestingAppId, sender_ids2);

  WaitUntilCompleted();
  WaitUntilCompleted();

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids),
            consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids2),
            consumer2()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Register a different app in a different profile.
  std::vector<std::string> sender_ids3;
  sender_ids3.push_back("sender1");
  sender_ids3.push_back("sender2");
  sender_ids3.push_back("sender3");
  consumer2()->Register(kTestingAppId2, sender_ids3);

  WaitUntilCompleted();

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids3),
            consumer2()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->registration_result());
}

TEST_F(GCMProfileServiceMultiProfileTest, Send) {
  // Send a message from one app in one profile.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  consumer()->Send(kTestingAppId, kUserId, message);

  // Send a message from same app in another profile.
  GCMClient::OutgoingMessage message2;
  message2.id = "2";
  message2.data["foo"] = "bar";
  consumer2()->Send(kTestingAppId, kUserId2, message2);

  WaitUntilCompleted();
  WaitUntilCompleted();

  EXPECT_EQ(consumer()->send_message_id(), message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());

  EXPECT_EQ(consumer2()->send_message_id(), message2.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->send_result());

  // Send another message from different app in another profile.
  GCMClient::OutgoingMessage message3;
  message3.id = "3";
  message3.data["hello"] = "world";
  consumer2()->Send(kTestingAppId2, kUserId, message3);

  WaitUntilCompleted();

  EXPECT_EQ(consumer2()->send_message_id(), message3.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());
}

TEST_F(GCMProfileServiceMultiProfileTest, MessageReceived) {
  consumer()->Register(kTestingAppId, ToSenderList("sender"));
  WaitUntilCompleted();
  consumer2()->Register(kTestingAppId, ToSenderList("sender"));
  WaitUntilCompleted();
  consumer2()->Register(kTestingAppId2, ToSenderList("sender2"));
  WaitUntilCompleted();

  // Trigger an incoming message for an app in one profile.
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);

  // Trigger an incoming message for the same app in another profile.
  GCMClient::IncomingMessage message2;
  message2.data["foo"] = "bar";
  message2.sender_id = "sender";
  consumer2()->GetGCMClient()->ReceiveMessage(kTestingAppId, message2);

  WaitUntilCompleted();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_app_handler()->message().data);
  EXPECT_EQ("sender", consumer()->gcm_app_handler()->message().sender_id);

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_app_handler()->app_id());
  EXPECT_TRUE(message2.data == consumer2()->gcm_app_handler()->message().data);
  EXPECT_EQ("sender", consumer2()->gcm_app_handler()->message().sender_id);

  // Trigger another incoming message for a different app in another profile.
  GCMClient::IncomingMessage message3;
  message3.data["bar1"] = "foo1";
  message3.data["bar2"] = "foo2";
  message3.sender_id = "sender2";
  consumer2()->GetGCMClient()->ReceiveMessage(kTestingAppId2, message3);

  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_app_handler()->app_id());
  EXPECT_TRUE(message3.data == consumer2()->gcm_app_handler()->message().data);
  EXPECT_EQ("sender2", consumer2()->gcm_app_handler()->message().sender_id);
}

// Test a set of GCM operations on multiple profiles.
// 1) Register 1 app in profile1 and register 2 apps in profile2;
// 2) Send a message from profile1;
// 3) Receive a message to an app in profile1 and receive a message for each of
//    two apps in profile2;
// 4) Send a message foe ach of two apps in profile2;
// 5) Sign out of profile1.
// 6) Register/send stops working for profile1;
// 7) The app in profile2 could still receive these events;
// 8) Sign into profile1 with a different user.
// 9) The message to the new signed-in user will be routed.
TEST_F(GCMProfileServiceMultiProfileTest, Combined) {
  // Register an app.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);

  // Register the same app in a different profile.
  std::vector<std::string> sender_ids2;
  sender_ids2.push_back("foo");
  sender_ids2.push_back("bar");
  consumer2()->Register(kTestingAppId, sender_ids2);

  // Register a different app in a different profile.
  std::vector<std::string> sender_ids3;
  sender_ids3.push_back("sender1");
  sender_ids3.push_back("sender2");
  sender_ids3.push_back("sender3");
  consumer2()->Register(kTestingAppId2, sender_ids3);

  WaitUntilCompleted();
  WaitUntilCompleted();
  WaitUntilCompleted();

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids),
            consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids3),
            consumer2()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // Send a message from one profile.
  GCMClient::OutgoingMessage out_message;
  out_message.id = "1";
  out_message.data["out1"] = "out_data1";
  out_message.data["out1_2"] = "out_data1_2";
  consumer()->Send(kTestingAppId, kUserId, out_message);

  WaitUntilCompleted();

  EXPECT_EQ(consumer()->send_message_id(), out_message.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->send_result());

  // Trigger an incoming message for an app in one profile.
  GCMClient::IncomingMessage in_message;
  in_message.data["in1"] = "in_data1";
  in_message.data["in1_2"] = "in_data1_2";
  in_message.sender_id = "sender1";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, in_message);

  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_app_handler()->app_id());
  EXPECT_TRUE(
      in_message.data == consumer()->gcm_app_handler()->message().data);

  // Trigger 2 incoming messages, one for each app respectively, in another
  // profile.
  GCMClient::IncomingMessage in_message2;
  in_message2.data["in2"] = "in_data2";
  in_message2.sender_id = "sender3";
  consumer2()->GetGCMClient()->ReceiveMessage(kTestingAppId2, in_message2);

  GCMClient::IncomingMessage in_message3;
  in_message3.data["in3"] = "in_data3";
  in_message3.data["in3_2"] = "in_data3_2";
  in_message3.sender_id = "foo";
  consumer2()->GetGCMClient()->ReceiveMessage(kTestingAppId, in_message3);

  consumer2()->gcm_app_handler()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_app_handler()->app_id());
  EXPECT_TRUE(
      in_message2.data == consumer2()->gcm_app_handler()->message().data);

  consumer2()->gcm_app_handler()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_app_handler()->app_id());
  EXPECT_TRUE(
      in_message3.data == consumer2()->gcm_app_handler()->message().data);

  // Send two messages, one for each app respectively, from another profile.
  GCMClient::OutgoingMessage out_message2;
  out_message2.id = "2";
  out_message2.data["out2"] = "out_data2";
  consumer2()->Send(kTestingAppId, kUserId2, out_message2);

  GCMClient::OutgoingMessage out_message3;
  out_message3.id = "2";
  out_message3.data["out3"] = "out_data3";
  consumer2()->Send(kTestingAppId2, kUserId2, out_message3);

  WaitUntilCompleted();

  EXPECT_EQ(consumer2()->send_message_id(), out_message2.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->send_result());

  WaitUntilCompleted();

  EXPECT_EQ(consumer2()->send_message_id(), out_message3.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->send_result());

  // Sign out of one profile.
  consumer()->SignOut();

  // Register/send stops working for signed-out profile.
  consumer()->gcm_app_handler()->clear_results();
  consumer()->Register(kTestingAppId, sender_ids);
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->registration_result());

  consumer()->gcm_app_handler()->clear_results();
  consumer()->Send(kTestingAppId2, kUserId2, out_message3);
  EXPECT_TRUE(consumer()->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->send_result());

  // Deleted messages event will go through for another signed-in profile.
  consumer2()->GetGCMClient()->DeleteMessages(kTestingAppId2);

  consumer2()->gcm_app_handler()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_app_handler()->app_id());

  // Send error event will go through for another signed-in profile.
  GCMClient::OutgoingMessage out_message4;
  out_message4.id = "1@error";
  out_message4.data["out4"] = "out_data4";
  consumer2()->Send(kTestingAppId, kUserId, out_message4);

  WaitUntilCompleted();
  EXPECT_EQ(consumer2()->send_message_id(), out_message4.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->send_result());

  consumer2()->gcm_app_handler()->clear_results();
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            consumer2()->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_app_handler()->app_id());
  EXPECT_EQ(out_message4.id,
            consumer2()->gcm_app_handler()->send_error_message_id());
  EXPECT_NE(GCMClient::SUCCESS,
            consumer2()->gcm_app_handler()->send_error_result());
  EXPECT_EQ(out_message4.data,
            consumer2()->gcm_app_handler()->send_error_data());

  // Sign in with a different user.
  consumer()->SignIn(kTestingUsername3);

  // Signing out cleared all registrations, so we need to register again.
  consumer()->Register(kTestingAppId, ToSenderList("sender1"));
  WaitUntilCompleted();

  // Incoming message will go through for the new signed-in user.
  GCMClient::IncomingMessage in_message5;
  in_message5.data["in5"] = "in_data5";
  in_message5.sender_id = "sender1";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, in_message5);

  consumer()->gcm_app_handler()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            consumer()->gcm_app_handler()->received_event());
  EXPECT_TRUE(
      in_message5.data == consumer()->gcm_app_handler()->message().data);
}

}  // namespace gcm
