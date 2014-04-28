// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_app_handler.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
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
const char kTestAccountID3[] = "user3@example.com";
const char kTestAppID1[] = "TestApp1";
const char kTestAppID2[] = "TestApp2";
const char kUserID1[] = "user1";
const char kUserID2[] = "user2";

void PumpCurrentLoop() {
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

class TestGCMService : public GCMService {
 public:
  TestGCMService(
      bool start_automatically,
      scoped_ptr<IdentityProvider> identity_provider,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  virtual ~TestGCMService();

 protected:
  // GCMService:
  virtual bool ShouldStartAutomatically() const OVERRIDE;
  virtual base::FilePath GetStorePath() const OVERRIDE;
  virtual scoped_refptr<net::URLRequestContextGetter>
      GetURLRequestContextGetter() const OVERRIDE;

 private:
  base::ScopedTempDir temp_dir_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  const bool start_automatically_;

  DISALLOW_COPY_AND_ASSIGN(TestGCMService);
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

TestGCMService::TestGCMService(
    bool start_automatically,
    scoped_ptr<IdentityProvider> identity_provider,
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : GCMService(identity_provider.Pass()),
      request_context_(request_context),
      start_automatically_(start_automatically) {
  if (!temp_dir_.CreateUniqueTempDir())
    ADD_FAILURE();
}

TestGCMService::~TestGCMService() {
}

bool TestGCMService::ShouldStartAutomatically() const {
  return start_automatically_;
}

base::FilePath TestGCMService::GetStorePath() const {
  return temp_dir_.path();
}

scoped_refptr<net::URLRequestContextGetter>
TestGCMService::GetURLRequestContextGetter() const {
  return request_context_;
}

}  // namespace

class TestGCMServiceWrapper {
 public:
  enum WaitToFinish {
    DO_NOT_WAIT,
    WAIT
  };

  explicit TestGCMServiceWrapper(
      const scoped_refptr<net::URLRequestContextGetter>& request_context);
  ~TestGCMServiceWrapper();

  TestGCMService* service() { return service_.get(); }
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

  bool ServiceHasAppHandlers() const;
  GCMClientMock* GetGCMClient();

  void CreateService(bool start_automatically,
                     GCMClientMock::LoadingDelay gcm_client_loading_delay);

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

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  FakeOAuth2TokenService token_service_;
  scoped_ptr<FakeIdentityProvider> identity_provider_owner_;
  FakeIdentityProvider* identity_provider_;
  scoped_ptr<TestGCMService> service_;
  scoped_ptr<FakeGCMAppHandler> gcm_app_handler_;

  base::Closure async_operation_completed_callback_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;
  GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(TestGCMServiceWrapper);
};

TestGCMServiceWrapper::TestGCMServiceWrapper(
    const scoped_refptr<net::URLRequestContextGetter>& request_context)
    : request_context_(request_context),
      identity_provider_(NULL),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR),
      unregistration_result_(GCMClient::UNKNOWN_ERROR) {
  identity_provider_owner_.reset(new FakeIdentityProvider(&token_service_));
  identity_provider_ = identity_provider_owner_.get();
}

TestGCMServiceWrapper::~TestGCMServiceWrapper() {
  if (!service_)
    return;

  service_->ShutdownService();
  service_.reset();
  PumpIOLoop();
}

void TestGCMServiceWrapper::ClearRegistrationResult() {
  registration_id_.clear();
  registration_result_ = GCMClient::UNKNOWN_ERROR;
}

void TestGCMServiceWrapper::ClearUnregistrationResult() {
  unregistration_result_ = GCMClient::UNKNOWN_ERROR;
}

bool TestGCMServiceWrapper::ServiceHasAppHandlers() const {
  return !service_->app_handlers_.empty();
}

GCMClientMock* TestGCMServiceWrapper::GetGCMClient() {
  return static_cast<GCMClientMock*>(service_->GetGCMClientForTesting());
}

void TestGCMServiceWrapper::CreateService(
    bool start_automatically,
    GCMClientMock::LoadingDelay gcm_client_loading_delay) {
  service_.reset(new TestGCMService(
      start_automatically,
      identity_provider_owner_.PassAs<IdentityProvider>(),
      request_context_));
  service_->Initialize(scoped_ptr<GCMClientFactory>(
      new FakeGCMClientFactory(gcm_client_loading_delay)));

  gcm_app_handler_.reset(new FakeGCMAppHandler);
  service_->AddAppHandler(kTestAppID1, gcm_app_handler_.get());
  service_->AddAppHandler(kTestAppID2, gcm_app_handler_.get());
}

void TestGCMServiceWrapper::SignIn(const std::string& account_id) {
  token_service_.AddAccount(account_id);
  identity_provider_->LogIn(account_id);
  PumpIOLoop();
  PumpUILoop();
}

void TestGCMServiceWrapper::SignOut() {
  identity_provider_->LogOut();
  PumpIOLoop();
  PumpUILoop();
}

void TestGCMServiceWrapper::Register(const std::string& app_id,
                                     const std::vector<std::string>& sender_ids,
                                     WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  service_->Register(app_id,
                     sender_ids,
                     base::Bind(&TestGCMServiceWrapper::RegisterCompleted,
                                base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void TestGCMServiceWrapper::Send(const std::string& app_id,
                                 const std::string& receiver_id,
                                 const GCMClient::OutgoingMessage& message,
                                 WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  service_->Send(app_id,
                 receiver_id,
                 message,
                 base::Bind(&TestGCMServiceWrapper::SendCompleted,
                            base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void TestGCMServiceWrapper::Unregister(const std::string& app_id,
                                       WaitToFinish wait_to_finish) {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  service_->Unregister(app_id,
                       base::Bind(&TestGCMServiceWrapper::UnregisterCompleted,
                                  base::Unretained(this)));
  if (wait_to_finish == WAIT)
    run_loop.Run();
}

void TestGCMServiceWrapper::WaitForAsyncOperation() {
  base::RunLoop run_loop;
  async_operation_completed_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

void TestGCMServiceWrapper::RegisterCompleted(
    const std::string& registration_id,
    GCMClient::Result result) {
  registration_id_ = registration_id;
  registration_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void TestGCMServiceWrapper::SendCompleted(const std::string& message_id,
                                          GCMClient::Result result) {
  send_message_id_ = message_id;
  send_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

void TestGCMServiceWrapper::UnregisterCompleted(GCMClient::Result result) {
  unregistration_result_ = result;
  if (!async_operation_completed_callback_.is_null())
    async_operation_completed_callback_.Run();
}

class GCMServiceTest : public testing::Test {
 protected:
  GCMServiceTest();
  virtual ~GCMServiceTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_ptr<TestGCMServiceWrapper> wrapper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMServiceTest);
};

GCMServiceTest::GCMServiceTest() {
}

GCMServiceTest::~GCMServiceTest() {
}

void GCMServiceTest::SetUp() {
  thread_bundle_.reset(new content::TestBrowserThreadBundle(
      content::TestBrowserThreadBundle::REAL_IO_THREAD));
  request_context_ = new net::TestURLRequestContextGetter(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
  wrapper_.reset(new TestGCMServiceWrapper(request_context_));
}

void GCMServiceTest::TearDown() {
  wrapper_.reset();
}

TEST_F(GCMServiceTest, CreateGCMServiceBeforeSignIn) {
  // Create CreateGMCService first.
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  EXPECT_FALSE(wrapper_->service()->IsStarted());

  // Sign in. This will kick off the check-in.
  wrapper_->SignIn(kTestAccountID1);
  EXPECT_TRUE(wrapper_->service()->IsStarted());
}

TEST_F(GCMServiceTest, CreateGCMServiceAfterSignIn) {
  // Sign in. This will not initiate the check-in.
  wrapper_->SignIn(kTestAccountID1);

  // Create GCMeService after sign-in.
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  EXPECT_TRUE(wrapper_->service()->IsStarted());
}

TEST_F(GCMServiceTest, Shutdown) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  EXPECT_TRUE(wrapper_->ServiceHasAppHandlers());

  wrapper_->service()->ShutdownService();
  EXPECT_FALSE(wrapper_->ServiceHasAppHandlers());
}

// These tests are flaky on Android. See http://crbug.com/367600.
#if defined(OS_ANDROID)
#define MAYBE_SignInAndSignOutUnderPositiveChannelSignal \
    DISABLED_SignInAndSignOutUnderPositiveChannelSignal
#define MAYBE_SignInAndSignOutUnderNonPositiveChannelSignal \
    DISABLED_SignInAndSignOutUnderNonPositiveChannelSignal
#define MAYBE_SignOutAndThenSignIn \
    DISABLED_SignOutAndThenSignIn
#define MAYBE_StopAndRestartGCM \
    DISABLED_StopAndRestartGCM
#else
#define MAYBE_SignInAndSignOutUnderPositiveChannelSignal \
    SignInAndSignOutUnderPositiveChannelSignal
#define MAYBE_SignInAndSignOutUnderNonPositiveChannelSignal \
    SignInAndSignOutUnderNonPositiveChannelSignal
#define MAYBE_SignOutAndThenSignIn \
    SignOutAndThenSignIn
#define MAYBE_StopAndRestartGCM \
    StopAndRestartGCM
#endif

TEST_F(GCMServiceTest, MAYBE_SignInAndSignOutUnderPositiveChannelSignal) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  wrapper_->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, wrapper_->GetGCMClient()->status());
}

TEST_F(GCMServiceTest, MAYBE_SignInAndSignOutUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  wrapper_->CreateService(false, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should not be loaded.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, wrapper_->GetGCMClient()->status());

  wrapper_->SignOut();

  // Check-out should still be performed.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, wrapper_->GetGCMClient()->status());
}

TEST_F(GCMServiceTest, MAYBE_SignOutAndThenSignIn) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  wrapper_->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, wrapper_->GetGCMClient()->status());

  // Sign-in with a different account.
  wrapper_->SignIn(kTestAccountID2);

  // GCMClient should be loaded again.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());
}

TEST_F(GCMServiceTest, MAYBE_StopAndRestartGCM) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should be loaded.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  // Stops the GCM.
  wrapper_->service()->Stop();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, wrapper_->GetGCMClient()->status());

  // Restarts the GCM.
  wrapper_->service()->Start();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be loaded.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  // Stops the GCM.
  wrapper_->service()->Stop();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::STOPPED, wrapper_->GetGCMClient()->status());

  // Sign out.
  wrapper_->SignOut();

  // GCMClient should be checked out.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::CHECKED_OUT, wrapper_->GetGCMClient()->status());
}

TEST_F(GCMServiceTest, RegisterWhenNotSignedIn) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  EXPECT_TRUE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->registration_result());
}

TEST_F(GCMServiceTest, RegisterUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  wrapper_->CreateService(false, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should not be checked in.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, wrapper_->GetGCMClient()->status());

  // Invoking register will make GCMClient checked in.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  // GCMClient should be checked in.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  // Registration should succeed.
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);
  EXPECT_EQ(expected_registration_id, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceTest, SendWhenNotSignedIn) {
  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  EXPECT_TRUE(wrapper_->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->send_result());
}

TEST_F(GCMServiceTest, SendUnderNonPositiveChannelSignal) {
  // Non-positive channel signal will prevent GCMClient from checking in during
  // sign-in.
  wrapper_->CreateService(false, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // GCMClient should not be checked in.
  EXPECT_FALSE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::UNINITIALIZED, wrapper_->GetGCMClient()->status());

  // Invoking send will make GCMClient checked in.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  // GCMClient should be checked in.
  EXPECT_TRUE(wrapper_->service()->IsGCMClientReady());
  EXPECT_EQ(GCMClientMock::LOADED, wrapper_->GetGCMClient()->status());

  // Sending should succeed.
  EXPECT_EQ(message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());
}

// Tests a single instance of GCMService.
class GCMServiceSingleInstanceTest : public GCMServiceTest {
 public:
  GCMServiceSingleInstanceTest();
  virtual ~GCMServiceSingleInstanceTest();

  // GCMServiceTest:
  virtual void SetUp() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMServiceSingleInstanceTest);
};

GCMServiceSingleInstanceTest::GCMServiceSingleInstanceTest() {
}

GCMServiceSingleInstanceTest::~GCMServiceSingleInstanceTest() {
}

void GCMServiceSingleInstanceTest::SetUp() {
  GCMServiceTest::SetUp();

  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);
}

TEST_F(GCMServiceSingleInstanceTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, RegisterError) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1@error");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  EXPECT_TRUE(wrapper_->registration_id().empty());
  EXPECT_NE(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, RegisterAgainWithSameSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  sender_ids.push_back("sender2");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  wrapper_->ClearRegistrationResult();

  // Calling register 2nd time with the same set of sender IDs but different
  // ordering will get back the same registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1,
                     another_sender_ids,
                     TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(expected_registration_id, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, RegisterAgainWithDifferentSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  const std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  const std::string expected_registration_id2 =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  // Calling register 2nd time with the different sender IDs will get back a new
  // registration ID.
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(expected_registration_id2, wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, GCMClientNotReadyBeforeRegistration) {
  // Make GCMClient not ready initially.
  wrapper_.reset(new TestGCMServiceWrapper(request_context_));
  wrapper_->CreateService(true, GCMClientMock::DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // The registration is on hold until GCMClient is ready.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1,
                     sender_ids,
                     TestGCMServiceWrapper::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_TRUE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, wrapper_->registration_result());

  // Register operation will be invoked after GCMClient becomes ready.
  wrapper_->GetGCMClient()->PerformDelayedLoading();
  wrapper_->WaitForAsyncOperation();
  EXPECT_FALSE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, RegisterAfterSignOut) {
  // This will trigger check-out.
  wrapper_->SignOut();

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  EXPECT_TRUE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, UnregisterExplicitly) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  EXPECT_FALSE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  wrapper_->Unregister(kTestAppID1, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->unregistration_result());
}

TEST_F(GCMServiceSingleInstanceTest, UnregisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  wrapper_->Register(kTestAppID1,
                     sender_ids,
                     TestGCMServiceWrapper::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is a
  // registration already in progress.
  wrapper_->Unregister(kTestAppID1, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            wrapper_->unregistration_result());

  // Complete the unregistration.
  wrapper_->WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  wrapper_->Unregister(kTestAppID1, TestGCMServiceWrapper::DO_NOT_WAIT);

  // Test that unregistration fails with async operation pending when there is
  // an unregistration already in progress.
  wrapper_->Unregister(kTestAppID1, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            wrapper_->unregistration_result());
  wrapper_->ClearUnregistrationResult();

  // Complete unregistration.
  wrapper_->WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->unregistration_result());
}

TEST_F(GCMServiceSingleInstanceTest, RegisterWhenAsyncOperationPending) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  // First start registration without waiting for it to complete.
  wrapper_->Register(kTestAppID1,
                     sender_ids,
                     TestGCMServiceWrapper::DO_NOT_WAIT);

  // Test that registration fails with async operation pending when there is a
  // registration already in progress.
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            wrapper_->registration_result());
  wrapper_->ClearRegistrationResult();

  // Complete the registration.
  wrapper_->WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  // Start unregistration without waiting for it to complete. This time no async
  // operation is pending.
  wrapper_->Unregister(kTestAppID1, TestGCMServiceWrapper::DO_NOT_WAIT);

  // Test that registration fails with async operation pending when there is an
  // unregistration already in progress.
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            wrapper_->registration_result());

  // Complete the first unregistration expecting success.
  wrapper_->WaitForAsyncOperation();
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->unregistration_result());

  // Test that it is ok to register again after unregistration.
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());
}

TEST_F(GCMServiceSingleInstanceTest, Send) {
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());
}

TEST_F(GCMServiceSingleInstanceTest, GCMClientNotReadyBeforeSending) {
  // Make GCMClient not ready initially.
  wrapper_.reset(new TestGCMServiceWrapper(request_context_));
  wrapper_->CreateService(true, GCMClientMock::DELAY_LOADING);
  wrapper_->SignIn(kTestAccountID1);

  // The sending is on hold until GCMClient is ready.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  wrapper_->Send(kTestAppID1,
                 kUserID1,
                 message,
                 TestGCMServiceWrapper::DO_NOT_WAIT);
  PumpIOLoop();
  PumpUILoop();

  EXPECT_TRUE(wrapper_->send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, wrapper_->send_result());

  // Send operation will be invoked after GCMClient becomes ready.
  wrapper_->GetGCMClient()->PerformDelayedLoading();
  wrapper_->WaitForAsyncOperation();
  EXPECT_EQ(message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());
}

TEST_F(GCMServiceSingleInstanceTest, SendAfterSignOut) {
  // This will trigger check-out.
  wrapper_->SignOut();

  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  EXPECT_TRUE(wrapper_->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->send_result());
}

TEST_F(GCMServiceSingleInstanceTest, SendError) {
  GCMClient::OutgoingMessage message;
  // Embedding error in id will tell the mock to simulate the send error.
  message.id = "1@error";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());

  // Wait for the send error.
  wrapper_->gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
  EXPECT_EQ(message.id,
            wrapper_->gcm_app_handler()->send_error_details().message_id);
  EXPECT_NE(GCMClient::SUCCESS,
            wrapper_->gcm_app_handler()->send_error_details().result);
  EXPECT_EQ(message.data,
            wrapper_->gcm_app_handler()->send_error_details().additional_data);
}

TEST_F(GCMServiceSingleInstanceTest, MessageReceived) {
  wrapper_->Register(kTestAppID1,
                     ToSenderList("sender"),
                     TestGCMServiceWrapper::WAIT);
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  wrapper_->GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  wrapper_->gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, wrapper_->gcm_app_handler()->message().data);
  EXPECT_TRUE(wrapper_->gcm_app_handler()->message().collapse_key.empty());
  EXPECT_EQ(message.sender_id,
            wrapper_->gcm_app_handler()->message().sender_id);
}

TEST_F(GCMServiceSingleInstanceTest, MessageWithCollapseKeyReceived) {
  wrapper_->Register(kTestAppID1,
                     ToSenderList("sender"),
                     TestGCMServiceWrapper::WAIT);
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.collapse_key = "collapse_key_value";
  message.sender_id = "sender";
  wrapper_->GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  wrapper_->gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, wrapper_->gcm_app_handler()->message().data);
  EXPECT_EQ(message.collapse_key,
            wrapper_->gcm_app_handler()->message().collapse_key);
}

TEST_F(GCMServiceSingleInstanceTest, MessagesDeleted) {
  wrapper_->GetGCMClient()->DeleteMessages(kTestAppID1);
  wrapper_->gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
}

// Tests to make sure that concurrent GCMService instances work correctly
// regardless how GCMClient is created.
class GCMServiceMultipleInstanceTest : public GCMServiceTest {
 protected:
  GCMServiceMultipleInstanceTest();
  virtual ~GCMServiceMultipleInstanceTest();

  // GCMServiceTest:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  scoped_ptr<TestGCMServiceWrapper> wrapper2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMServiceMultipleInstanceTest);
};

GCMServiceMultipleInstanceTest::GCMServiceMultipleInstanceTest() {
}

GCMServiceMultipleInstanceTest::~GCMServiceMultipleInstanceTest() {
}

void GCMServiceMultipleInstanceTest::SetUp() {
  GCMServiceTest::SetUp();

  wrapper2_.reset(new TestGCMServiceWrapper(request_context_));

  wrapper_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);
  wrapper2_->CreateService(true, GCMClientMock::NO_DELAY_LOADING);

  // Initiate check-in for each instance.
  wrapper_->SignIn(kTestAccountID1);
  wrapper2_->SignIn(kTestAccountID2);
}

void GCMServiceMultipleInstanceTest::TearDown() {
  wrapper2_.reset();
}

TEST_F(GCMServiceMultipleInstanceTest, Register) {
  // Register an app.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  // Register the same app in a different instance.
  std::vector<std::string> sender_ids2;
  sender_ids2.push_back("foo");
  sender_ids2.push_back("bar");
  wrapper2_->Register(kTestAppID1, sender_ids2, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids),
            wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids2),
            wrapper2_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->registration_result());

  // Register a different app in a different instance.
  std::vector<std::string> sender_ids3;
  sender_ids3.push_back("sender1");
  sender_ids3.push_back("sender2");
  sender_ids3.push_back("sender3");
  wrapper2_->Register(kTestAppID2, sender_ids3, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids3),
            wrapper2_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->registration_result());
}

TEST_F(GCMServiceMultipleInstanceTest, Send) {
  // Send a message from one app in one instance.
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  wrapper_->Send(kTestAppID1, kUserID1, message, TestGCMServiceWrapper::WAIT);

  // Send a message from same app in another instance.
  GCMClient::OutgoingMessage message2;
  message2.id = "2";
  message2.data["foo"] = "bar";
  wrapper2_->Send(kTestAppID1, kUserID2, message2, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());

  EXPECT_EQ(message2.id, wrapper2_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->send_result());

  // Send another message from different app in another instance.
  GCMClient::OutgoingMessage message3;
  message3.id = "3";
  message3.data["hello"] = "world";
  wrapper2_->Send(kTestAppID2, kUserID1, message3, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(message3.id, wrapper2_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->send_result());
}

TEST_F(GCMServiceMultipleInstanceTest, MessageReceived) {
  wrapper_->Register(kTestAppID1,
                     ToSenderList("sender"),
                     TestGCMServiceWrapper::WAIT);
  wrapper2_->Register(kTestAppID1,
                      ToSenderList("sender"),
                      TestGCMServiceWrapper::WAIT);
  wrapper2_->Register(kTestAppID2,
                      ToSenderList("sender2"),
                      TestGCMServiceWrapper::WAIT);

  // Trigger an incoming message for an app in one instance.
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender";
  wrapper_->GetGCMClient()->ReceiveMessage(kTestAppID1, message);
  wrapper_->gcm_app_handler()->WaitForNotification();

  // Trigger an incoming message for the same app in another instance.
  GCMClient::IncomingMessage message2;
  message2.data["foo"] = "bar";
  message2.sender_id = "sender";
  wrapper2_->GetGCMClient()->ReceiveMessage(kTestAppID1, message2);
  wrapper2_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
  EXPECT_EQ(message.data, wrapper_->gcm_app_handler()->message().data);
  EXPECT_EQ("sender", wrapper_->gcm_app_handler()->message().sender_id);

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper2_->gcm_app_handler()->app_id());
  EXPECT_EQ(message2.data, wrapper2_->gcm_app_handler()->message().data);
  EXPECT_EQ("sender", wrapper2_->gcm_app_handler()->message().sender_id);

  // Trigger another incoming message for a different app in another instance.
  GCMClient::IncomingMessage message3;
  message3.data["bar1"] = "foo1";
  message3.data["bar2"] = "foo2";
  message3.sender_id = "sender2";
  wrapper2_->GetGCMClient()->ReceiveMessage(kTestAppID2, message3);
  wrapper2_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID2, wrapper2_->gcm_app_handler()->app_id());
  EXPECT_EQ(message3.data, wrapper2_->gcm_app_handler()->message().data);
  EXPECT_EQ("sender2", wrapper2_->gcm_app_handler()->message().sender_id);
}

// Test a set of GCM operations on multiple instances.
// 1) Register 1 app in instance 1 and register 2 apps in instance 2;
// 2) Send a message from instance 1;
// 3) Receive a message to an app in instance 1 and receive a message for each
//    of the two apps in instance 2;
// 4) Send a message for each of the two apps in instance 2;
// 5) Sign out of instance 1.
// 6) Register/send stops working for instance 1;
// 7) The app in instance 2 can still receive these events;
// 8) Sign into instance 1 with a different account.
// 9) The message to the newly signed-in account will be routed.
TEST_F(GCMServiceMultipleInstanceTest, Combined) {
  // Register an app.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);

  // Register the same app in a different instance.
  std::vector<std::string> sender_ids2;
  sender_ids2.push_back("foo");
  sender_ids2.push_back("bar");
  wrapper2_->Register(kTestAppID1, sender_ids2, TestGCMServiceWrapper::WAIT);

  // Register a different app in a different instance.
  std::vector<std::string> sender_ids3;
  sender_ids3.push_back("sender1");
  sender_ids3.push_back("sender2");
  sender_ids3.push_back("sender3");
  wrapper2_->Register(kTestAppID2, sender_ids3, TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids),
            wrapper_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->registration_result());

  EXPECT_EQ(GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids3),
            wrapper2_->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->registration_result());

  // Send a message from one instance.
  GCMClient::OutgoingMessage out_message;
  out_message.id = "1";
  out_message.data["out1"] = "out_data1";
  out_message.data["out1_2"] = "out_data1_2";
  wrapper_->Send(kTestAppID1,
                 kUserID1,
                 out_message,
                 TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(out_message.id, wrapper_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper_->send_result());

  // Trigger an incoming message for an app in one instance.
  GCMClient::IncomingMessage in_message;
  in_message.data["in1"] = "in_data1";
  in_message.data["in1_2"] = "in_data1_2";
  in_message.sender_id = "sender1";
  wrapper_->GetGCMClient()->ReceiveMessage(kTestAppID1, in_message);
  wrapper_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper_->gcm_app_handler()->app_id());
  EXPECT_EQ(in_message.data, wrapper_->gcm_app_handler()->message().data);

  // Trigger 2 incoming messages, one for each app respectively, in another
  // instance.
  GCMClient::IncomingMessage in_message2;
  in_message2.data["in2"] = "in_data2";
  in_message2.sender_id = "sender3";
  wrapper2_->GetGCMClient()->ReceiveMessage(kTestAppID2, in_message2);

  GCMClient::IncomingMessage in_message3;
  in_message3.data["in3"] = "in_data3";
  in_message3.data["in3_2"] = "in_data3_2";
  in_message3.sender_id = "foo";
  wrapper2_->GetGCMClient()->ReceiveMessage(kTestAppID1, in_message3);

  wrapper2_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID2, wrapper2_->gcm_app_handler()->app_id());
  EXPECT_EQ(in_message2.data, wrapper2_->gcm_app_handler()->message().data);

  wrapper2_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper2_->gcm_app_handler()->app_id());
  EXPECT_EQ(in_message3.data, wrapper2_->gcm_app_handler()->message().data);

  // Send two messages, one for each app respectively, from another instance.
  GCMClient::OutgoingMessage out_message2;
  out_message2.id = "2";
  out_message2.data["out2"] = "out_data2";
  wrapper2_->Send(kTestAppID1,
                  kUserID2,
                  out_message2,
                  TestGCMServiceWrapper::WAIT);

  GCMClient::OutgoingMessage out_message3;
  out_message3.id = "3";
  out_message3.data["out3"] = "out_data3";
  wrapper2_->Send(kTestAppID2,
                  kUserID2,
                  out_message3,
                  TestGCMServiceWrapper::DO_NOT_WAIT);

  EXPECT_EQ(out_message2.id, wrapper2_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->send_result());

  wrapper2_->WaitForAsyncOperation();

  EXPECT_EQ(out_message3.id, wrapper2_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->send_result());

  // Sign out of one instance.
  wrapper_->SignOut();

  // Register/send stops working for signed-out instance.
  wrapper_->Register(kTestAppID1, sender_ids, TestGCMServiceWrapper::WAIT);
  EXPECT_TRUE(wrapper_->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->registration_result());

  wrapper_->Send(kTestAppID2,
                 kUserID2,
                 out_message3,
                 TestGCMServiceWrapper::WAIT);
  EXPECT_TRUE(wrapper_->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, wrapper_->send_result());

  // Deleted messages event will go through for another signed-in instance.
  wrapper2_->GetGCMClient()->DeleteMessages(kTestAppID2);
  wrapper2_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGES_DELETED_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID2, wrapper2_->gcm_app_handler()->app_id());

  // Send error event will go through for another signed-in instance.
  GCMClient::OutgoingMessage out_message4;
  out_message4.id = "1@error";
  out_message4.data["out4"] = "out_data4";
  wrapper2_->Send(kTestAppID1,
                  kUserID1,
                  out_message4,
                  TestGCMServiceWrapper::WAIT);

  EXPECT_EQ(out_message4.id, wrapper2_->send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, wrapper2_->send_result());

  wrapper2_->gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(FakeGCMAppHandler::SEND_ERROR_EVENT,
            wrapper2_->gcm_app_handler()->received_event());
  EXPECT_EQ(kTestAppID1, wrapper2_->gcm_app_handler()->app_id());
  EXPECT_EQ(out_message4.id,
            wrapper2_->gcm_app_handler()->send_error_details().message_id);
  EXPECT_NE(GCMClient::SUCCESS,
            wrapper2_->gcm_app_handler()->send_error_details().result);
  EXPECT_EQ(out_message4.data,
            wrapper2_->gcm_app_handler()->send_error_details().additional_data);

  // Sign in with a different account.
  wrapper_->SignIn(kTestAccountID3);

  // Signing out cleared all registrations, so we need to register again.
  wrapper_->Register(kTestAppID1,
                     ToSenderList("sender1"),
                     TestGCMServiceWrapper::WAIT);

  // Incoming message will go through for the new signed-in account.
  GCMClient::IncomingMessage in_message5;
  in_message5.data["in5"] = "in_data5";
  in_message5.sender_id = "sender1";
  wrapper_->GetGCMClient()->ReceiveMessage(kTestAppID1, in_message5);

  wrapper_->gcm_app_handler()->WaitForNotification();

  EXPECT_EQ(FakeGCMAppHandler::MESSAGE_EVENT,
            wrapper_->gcm_app_handler()->received_event());
  EXPECT_EQ(in_message5.data, wrapper_->gcm_app_handler()->message().data);
}

}  // namespace gcm
