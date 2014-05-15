// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_driver.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_app_handler.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kTestAccountID1[] = "user1@example.com";
const char kTestAccountID2[] = "user2@example.com";
const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";
const char kUserID1[] = "user1";

void PumpCurrentLoop() {
  base::MessageLoop::ScopedNestableTaskAllower
      nestable_task_allower(base::MessageLoop::current());
  base::RunLoop().RunUntilIdle();
}

void PumpUILoop() {
  PumpCurrentLoop();
}

void PumpIOLoop() {
  base::RunLoop run_loop;
  content::BrowserThread::PostTaskAndReply(content::BrowserThread::IO,
                                           FROM_HERE,
                                           base::Bind(&PumpCurrentLoop),
                                           run_loop.QuitClosure());
  run_loop.Run();
}

std::vector<std::string> ToSenderList(const std::string& sender_ids) {
  std::vector<std::string> senders;
  Tokenize(sender_ids, ",", &senders);
  return senders;
}

class FakeGCMAppHandler : public GCMAppHandler {
 public:
  enum Event {
    NO_EVENT,
    MESSAGE_EVENT,
    MESSAGES_DELETED_EVENT,
    SEND_ERROR_EVENT
  };

  FakeGCMAppHandler();
  virtual ~FakeGCMAppHandler();

  const Event& received_event() const { return received_event_; }
  const std::string& app_id() const { return app_id_; }
  const GCMClient::IncomingMessage& message() const { return message_; }
  const GCMClient::SendErrorDetails& send_error_details() const {
    return send_error_details_;
  }

  void WaitForNotification();

  // GCMAppHandler:
  virtual void ShutdownHandler() OVERRIDE;
  virtual void OnMessage(const std::string& app_id,
                         const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE;

 private:
  void ClearResults();

  scoped_ptr<base::RunLoop> run_loop_;

  Event received_event_;
  std::string app_id_;
  GCMClient::IncomingMessage message_;
  GCMClient::SendErrorDetails send_error_details_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMAppHandler);
};

class TestGCMDriver : public GCMDriver {
 public:
  TestGCMDriver(
      bool start_automatically,
      scoped_ptr<IdentityProvider> identity_provider,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  virtual ~TestGCMDriver();

 protected:
  // GCMDriver:
  virtual bool ShouldStartAutomatically() const OVERRIDE;
  virtual base::FilePath GetStorePath() const OVERRIDE;
  virtual scoped_refptr<net::URLRequestContextGetter>
      GetURLRequestContextGetter() const OVERRIDE;

 private:
  base::ScopedTempDir temp_dir_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  const bool start_automatically_;

  DISALLOW_COPY_AND_ASSIGN(TestGCMDriver);
};

FakeGCMAppHandler::FakeGCMAppHandler() : received_event_(NO_EVENT) {
}

FakeGCMAppHandler::~FakeGCMAppHandler() {
}

void FakeGCMAppHandler::WaitForNotification() {
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();
  run_loop_.reset();
}

void FakeGCMAppHandler::ShutdownHandler() {
}

void FakeGCMAppHandler::OnMessage(const std::string& app_id,
                                  const GCMClient::IncomingMessage& message) {
  ClearResults();
  received_event_ = MESSAGE_EVENT;
  app_id_ = app_id;
  message_ = message;
  if (run_loop_)
    run_loop_->Quit();
}

void FakeGCMAppHandler::OnMessagesDeleted(const std::string& app_id) {
  ClearResults();
  received_event_ = MESSAGES_DELETED_EVENT;
  app_id_ = app_id;
  if (run_loop_)
    run_loop_->Quit();
}

void FakeGCMAppHandler::OnSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  ClearResults();
  received_event_ = SEND_ERROR_EVENT;
  app_id_ = app_id;
  send_error_details_ = send_error_details;
  if (run_loop_)
    run_loop_->Quit();
}

void FakeGCMAppHandler::ClearResults() {
  received_event_ = NO_EVENT;
  app_id_.clear();
  message_ = GCMClient::IncomingMessage();
  send_error_details_ = GCMClient::SendErrorDetails();
}

TestGCMDriver::TestGCMDriver(
    bool start_automatically,
    scoped_ptr<IdentityProvider> identity_provider,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : GCMDriver(identity_provider.Pass()),
      request_context_(request_context),
      start_automatically_(start_automatically) {
  if (!temp_dir_.CreateUniqueTempDir())
    ADD_FAILURE();
}

TestGCMDriver::~TestGCMDriver() {
}

bool TestGCMDriver::ShouldStartAutomatically() const {
  return start_automatically_;
}

base::FilePath TestGCMDriver::GetStorePath() const {
  return temp_dir_.path();
}

scoped_refptr<net::URLRequestContextGetter>
TestGCMDriver::GetURLRequestContextGetter() const {
  return request_context_;
}

}  // namespace

class GCMDriverTest : public testing::Test {
 public:
  enum WaitToFinish {
    DO_NOT_WAIT,
    WAIT
  };

  GCMDriverTest();
  virtual ~GCMDriverTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  TestGCMDriver* driver() { return driver_.get(); }
  FakeGCMAppHandler* gcm_app_handler() { return gcm_app_handler_.get(); }
  const std::string& registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  const std::string& send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }
  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

  void ClearRegistrationResult();
  void ClearUnregistrationResult();

  bool HasAppHandlers() const;
  GCMClientMock* GetGCMClient();

  void CreateDriver(bool start_automatically,
                    GCMClientMock::StartMode gcm_client_start_mode);

  void SignIn(const std::string& account_id);
  void SignOut();

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                WaitToFinish wait_to_finish);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message,
            WaitToFinish wait_to_finish);
  void Unregister(const std::string& app_id, WaitToFinish wait_to_finish);

  void WaitForAsyncOperation();

 private:
  void RegisterCompleted(const std::string& registration_id,
                         GCMClient::Result result);
  void SendCompleted(const std::string& message_id, GCMClient::Result result);
  void UnregisterCompleted(GCMClient::Result result);

  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  FakeOAuth2TokenService token_service_;
  scoped_ptr<FakeIdentityProvider> identity_provider_owner_;
  FakeIdentityProvider* identity_provider_;
  scoped_ptr<TestGCMDriver> driver_;
  scoped_ptr<FakeGCMAppHandler> gcm_app_handler_;

  base::Closure async_operation_completed_callback_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;
  GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverTest);
};

GCMDriverTest::GCMDriverTest()
    : identity_provider_(NULL),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR),
      unregistration_result_(GCMClient::UNKNOWN_ERROR) {
  identity_provider_owner_.reset(new FakeIdentityProvider(&token_service_));
  identity_provider_ = identity_provider_owner_.get();
}

GCMDriverTest::~GCMDriverTest() {
}

void GCMDriverTest::SetUp() {
  thread_bundle_.reset(new content::TestBrowserThreadBundle(
      content::TestBrowserThreadBundle::REAL_IO_THREAD));
}

void GCMDriverTest::TearDown() {
  if (!driver_)
    return;

  driver_->ShutdownService();
  driver_.reset();
  PumpIOLoop();
}

void GCMDriverTest::ClearRegistrationResult() {
  registration_id_.clear();
  registration_result_ = GCMClient::UNKNOWN_ERROR;
}

void GCMDriverTest::ClearUnregistrationResult() {
  unregistration_result_ = GCMClient::UNKNOWN_ERROR;
}

bool GCMDriverTest::HasAppHandlers() const {
  return !driver_->app_handlers().empty();
}

GCMClientMock* GCMDriverTest::GetGCMClient() {
  return static_cast<GCMClientMock*>(driver_->GetGCMClientForTesting());
}

void GCMDriverTest::CreateDriver(
    bool start_automatically,
    GCMClientMock::StartMode gcm_client_start_mode) {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::IO));
  driver_.reset(new TestGCMDriver(
      start_automatically,
      identity_provider_owner_.PassAs<IdentityProvider>(),
      request_context));
  driver_->Initialize(scoped_ptr<GCMClientFactory>(
      new FakeGCMClientFactory(gcm_client_start_mode)));

  gcm_app_handler_.reset(new FakeGCMAppHandler);
  driver_->AddAppHandler(kTestAppID1, gcm_app_handler_.get());
  driver_->AddAppHandler(kTestAppID2, gcm_app_handler_.get());
}

void GCMDriverTest::SignIn(const std::string& account_id) {
  token_service_.AddAccount(account_id);
  identity_provider_->LogIn(account_id);
  PumpIOLoop();
  PumpUILoop();
}

void GCMDriverTest::SignOut() {
  identity_provider_->LogOut();
  PumpIOLoop();
  PumpUILoop();
}

void GCMDriverTest::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids,
                             WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Register(app_id,
                     sender_ids,
                     base::Bind(&GCMDriverTest::RegisterCompleted,
                                base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const GCMClient::OutgoingMessage& message,
                         WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Send(app_id,
                 receiver_id,
                 message,
                 base::Bind(&GCMDriverTest::SendCompleted,
                            base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::Unregister(const std::string& app_id,
                               WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  driver_->Unregister(app_id,
                       base::Bind(&GCMDriverTest::UnregisterCompleted,
                                  base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void GCMDriverTest::WaitForAsyncOperation() {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void GCMDriverTest::RegisterCompleted(const std::string& registration_id,
                                      GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void GCMDriverTest::SendCompleted(const std::string& message_id,
                                  GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void GCMDriverTest::UnregisterCompleted(GCMClient::Result result) {
  unregistration_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

TEST_F(GCMDriverTest, CreateGCMDriverBeforeSignIn) {
  // Create CreateGMCService first.
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  EXPECT_FALSE(driver()->IsStarted());

  // Sign in. This will kick off the check-in.
  SignIn(kTestAccountID1);
  EXPECT_TRUE(driver()->IsStarted());
}

TEST_F(GCMDriverTest, CreateGCMDriverAfterSignIn) {
  // Sign in. This will not initiate the check-in.
  SignIn(kTestAccountID1);

  // Create GCMeService after sign-in.
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  EXPECT_TRUE(driver()->IsStarted());
}

TEST_F(GCMDriverTest, Shutdown) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  EXPECT_TRUE(HasAppHandlers());

  driver()->ShutdownService();
  EXPECT_FALSE(HasAppHandlers());
}

TEST_F(GCMDriverTest, SignInAndSignOutUnderPositiveChannelSignal) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, SignInAndSignOutUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  CreateDriver(false, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should not be loaded.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, GetGCMClient()->status());

  SignOut();

  // Check-out should still be performed.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, SignOutAndThenSignIn) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, GetGCMClient()->status());

  // Sign-in with a different account.
  SignIn(kTestAccountID2);

  // GCMClient should be loaded again.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, StopAndRestartGCM) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  // Stops the GCM.
  driver()->Stop();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, GetGCMClient()->status());

  // Restarts the GCM.
  driver()->Start();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be loaded.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  // Stops the GCM.
  driver()->Stop();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, GetGCMClient()->status());

  // Sign out.
  SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, RegisterWhenNotSignedIn) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, registration_result());
}

TEST_F(GCMDriverTest, RegisterUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  CreateDriver(false, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should not be checked in.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, GetGCMClient()->status());

  // Invoking register will make GCMClient checked in.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  // GCMClient should be checked in.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  // Registration should succeed.
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);
  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverTest, SendWhenNotSignedIn) {
  CreateDriver(true, GCMClientMock::NO_DELAY_START);

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, send_result());
}

TEST_F(GCMDriverTest, SendUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  CreateDriver(false, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient should not be checked in.
  EXPECT_FALSE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, GetGCMClient()->status());

  // Invoking send will make GCMClient checked in.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  // GCMClient should be checked in.
  EXPECT_TRUE(driver()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STARTED, GetGCMClient()->status());

  // Sending should succeed.
  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());
}

TEST_F(GCMDriverTest, GCMClientNotReadyBeforeRegistration) {
  // Make GCMClient not ready initially.
  CreateDriver(true, GCMClientMock::DELAY_START);
  SignIn(kTestAccountID1);

  // The registration is on hold until GCMClient is ready.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1,
                     sender_ids,
                     GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, registration_result());

  // Register operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedLoading();
  WaitForAsyncOperation();
  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverTest, GCMClientNotReadyBeforeSending) {
  // Make GCMClient not ready initially.
  CreateDriver(true, GCMClientMock::DELAY_START);
  SignIn(kTestAccountID1);

  // The sending is on hold until GCMClient is ready.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();

  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, send_result());

  // Send operation will be invoked after GCMClient becomes ready.
  GetGCMClient()->PerformDelayedLoading();
  WaitForAsyncOperation();
  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());
}

// Tests a single instance of GCMDriver.
class GCMDriverFunctionalTest : public GCMDriverTest {
 public:
  GCMDriverFunctionalTest();
  virtual ~GCMDriverFunctionalTest();

  // GCMDriverTest:
  virtual void SetUp() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMDriverFunctionalTest);
};

GCMDriverFunctionalTest::GCMDriverFunctionalTest() {
}

GCMDriverFunctionalTest::~GCMDriverFunctionalTest() {
}

void GCMDriverFunctionalTest::SetUp() {
  GCMDriverTest::SetUp();

  CreateDriver(true, GCMClientMock::NO_DELAY_START);
  SignIn(kTestAccountID1);
}

TEST_F(GCMDriverFunctionalTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterError) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1@error");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_TRUE(registration_id().empty());
  EXPECT_NE(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAgainWithSameSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  sender_ids.push_back("sender2");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  ClearRegistrationResult();

  // Calling register 2nd time with the same set of sender IDs but different
  // ordering will get back the same registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  Register(kTestAppID1, another_sender_ids, GCMDriverTest::WAIT);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAgainWithDifferentSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  const std::string expected_registration_id2 =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  // Calling register 2nd time with the different sender IDs will get back a new
  // registration ID.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(expected_registration_id2, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterAfterSignOut) {
  // This will trigger check-out.
  SignOut();

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, registration_result());
}

TEST_F(GCMDriverFunctionalTest, UnregisterExplicitly) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);

  EXPECT_FALSE(registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  Unregister(kTestAppID1, GCMDriverTest::WAIT);

  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

TEST_F(GCMDriverFunctionalTest, UnregisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  Register(kTestAppID1,
                     sender_ids,
                     GCMDriverTest::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is a
  // registration already in progress.
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            unregistration_result());

  // Complete the unregistration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  Unregister(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is
  // an unregistration already in progress.
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            unregistration_result());
  ClearUnregistrationResult();

  // Complete unregistration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());
}

TEST_F(GCMDriverFunctionalTest, RegisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  Register(kTestAppID1,
                     sender_ids,
                     GCMDriverTest::DO_NOT_WAIT);

  // Test that registration fails with async operation pending when there is a
  // registration already in progress.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            registration_result());
  ClearRegistrationResult();

  // Complete the registration.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  Unregister(kTestAppID1, GCMDriverTest::DO_NOT_WAIT);

  // Test that registration fails with async operation pending when there is an
  // unregistration already in progress.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            registration_result());

  // Complete the first unregistration expecting success.
  WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, unregistration_result());

  // Test that it is ok to register again after unregistration.
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());
}

TEST_F(GCMDriverFunctionalTest, Send) {
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());
}

TEST_F(GCMDriverFunctionalTest, SendAfterSignOut) {
  // This will trigger check-out.
  SignOut();

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, send_result());
}

TEST_F(GCMDriverFunctionalTest, SendError) {
  GCMClient::OutgoingMessage message;
  // Embedding error in id will tell the mock to simulate the send error.
  message.id = "1@error";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());

  // Wait for the send error.
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.id,
            gcm_app_handler()->send_error_details().message_id);
  EXPECT_NE(GCMClient::SUCCESS,
            gcm_app_handler()->send_error_details().result);
  EXPECT_EQ(message.data,
            gcm_app_handler()->send_error_details().additional_data);
}

TEST_F(GCMDriverFunctionalTest, MessageReceived) {
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, gcm_app_handler()->message().data);
  EXPECT_TRUE(gcm_app_handler()->message().collapse_key.empty());
  EXPECT_EQ(message.sender_id, gcm_app_handler()->message().sender_id);
}

TEST_F(GCMDriverFunctionalTest, MessageWithCollapseKeyReceived) {
  Register(kTestAppID1, ToSenderList("sender"), GCMDriverTest::WAIT);
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.collapse_key = "collapse_key_value";
  message.sender_id = "sender";
  GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, gcm_app_handler()->message().data);
  EXPECT_EQ(message.collapse_key,
            gcm_app_handler()->message().collapse_key);
}

TEST_F(GCMDriverFunctionalTest, MessagesDeleted) {
  GetGCMClient()->DeleteMessages(kTestAppID1);
  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
}

}  // namespace gcm
