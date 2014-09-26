// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "components/gcm_driver/fake_gcm_app_handler.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/fake_gcm_client_factory.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_channel_status_request.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/gcm_driver/proto/gcm_channel_status.pb.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
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

class FakeGCMConnectionObserver : public GCMConnectionObserver {
 public:
  FakeGCMConnectionObserver();
  virtual ~FakeGCMConnectionObserver();

  // gcm::GCMConnectionObserver implementation:
  virtual void OnConnected(const net::IPEndPoint& ip_endpoint) OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

  bool connected() const { return connected_; }

 private:
  bool connected_;
};

FakeGCMConnectionObserver::FakeGCMConnectionObserver() : connected_(false) {
}

FakeGCMConnectionObserver::~FakeGCMConnectionObserver() {
}

void FakeGCMConnectionObserver::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  connected_ = true;
}

void FakeGCMConnectionObserver::OnDisconnected() {
  connected_ = false;
}

void PumpCurrentLoop() {
  base::MessageLoop::ScopedNestableTaskAllower
      nestable_task_allower(base::MessageLoop::current());
  base::RunLoop().RunUntilIdle();
}

void PumpUILoop() {
  PumpCurrentLoop();
}

std::vector<std::string> ToSenderList(const std::string& sender_ids) {
  std::vector<std::string> senders;
  Tokenize(sender_ids, ",", &senders);
  return senders;
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

  GCMDriverDesktop* driver() { return driver_.get(); }
  FakeGCMAppHandler* gcm_app_handler() { return gcm_app_handler_.get(); }
  FakeGCMConnectionObserver* gcm_connection_observer() {
    return gcm_connection_observer_.get();
  }
  const std::string& registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  const std::string& send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }
  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

  void PumpIOLoop();

  void ClearResults();

  bool HasAppHandlers() const;
  FakeGCMClient* GetGCMClient();

  void CreateDriver(FakeGCMClient::StartMode gcm_client_start_mode);
  void ShutdownDriver();
  void AddAppHandlers();
  void RemoveAppHandlers();

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

  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple prefs_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::MessageLoopForUI message_loop_;
  base::Thread io_thread_;
  base::FieldTrialList field_trial_list_;
  scoped_ptr<GCMDriverDesktop> driver_;
  scoped_ptr<FakeGCMAppHandler> gcm_app_handler_;
  scoped_ptr<FakeGCMConnectionObserver> gcm_connection_observer_;

  base::Closure async_operation_completed_callback_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  std::string send_message_id_;
  GCMClient::Result send_result_;
  GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(GCMDriverTest);
};

GCMDriverTest::GCMDriverTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      io_thread_("IOThread"),
      field_trial_list_(NULL),
      registration_result_(GCMClient::UNKNOWN_ERROR),
      send_result_(GCMClient::UNKNOWN_ERROR),
      unregistration_result_(GCMClient::UNKNOWN_ERROR) {
}

GCMDriverTest::~GCMDriverTest() {
}

void GCMDriverTest::SetUp() {
  GCMChannelStatusSyncer::RegisterPrefs(prefs_.registry());
  io_thread_.Start();
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void GCMDriverTest::TearDown() {
  if (!driver_)
    return;

  ShutdownDriver();
  driver_.reset();
  PumpIOLoop();

  io_thread_.Stop();
}

void GCMDriverTest::PumpIOLoop() {
  base::RunLoop run_loop;
  io_thread_.message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&PumpCurrentLoop),
      run_loop.QuitClosure());
  run_loop.Run();
}

void GCMDriverTest::ClearResults() {
  registration_id_.clear();
  registration_result_ = GCMClient::UNKNOWN_ERROR;

  send_message_id_.clear();
  send_result_ = GCMClient::UNKNOWN_ERROR;

  unregistration_result_ = GCMClient::UNKNOWN_ERROR;
}

bool GCMDriverTest::HasAppHandlers() const {
  return !driver_->app_handlers().empty();
}

FakeGCMClient* GCMDriverTest::GetGCMClient() {
  return static_cast<FakeGCMClient*>(driver_->GetGCMClientForTesting());
}

void GCMDriverTest::CreateDriver(
    FakeGCMClient::StartMode gcm_client_start_mode) {
  scoped_refptr<net::URLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(io_thread_.message_loop_proxy());
  // TODO(johnme): Need equivalent test coverage of GCMDriverAndroid.
  driver_.reset(new GCMDriverDesktop(
      scoped_ptr<GCMClientFactory>(new FakeGCMClientFactory(
          gcm_client_start_mode,
          base::MessageLoopProxy::current(),
          io_thread_.message_loop_proxy())).Pass(),
      GCMClient::ChromeBuildInfo(),
      &prefs_,
      temp_dir_.path(),
      request_context,
      base::MessageLoopProxy::current(),
      io_thread_.message_loop_proxy(),
      task_runner_));

  gcm_app_handler_.reset(new FakeGCMAppHandler);
  gcm_connection_observer_.reset(new FakeGCMConnectionObserver);

  driver_->AddConnectionObserver(gcm_connection_observer_.get());
}

void GCMDriverTest::ShutdownDriver() {
  if (gcm_connection_observer())
    driver()->RemoveConnectionObserver(gcm_connection_observer());
  driver()->Shutdown();
}

void GCMDriverTest::AddAppHandlers() {
  driver_->AddAppHandler(kTestAppID1, gcm_app_handler_.get());
  driver_->AddAppHandler(kTestAppID2, gcm_app_handler_.get());
}

void GCMDriverTest::RemoveAppHandlers() {
  driver_->RemoveAppHandler(kTestAppID1);
  driver_->RemoveAppHandler(kTestAppID2);
}

void GCMDriverTest::SignIn(const std::string& account_id) {
  driver_->OnSignedIn();
  PumpIOLoop();
  PumpUILoop();
}

void GCMDriverTest::SignOut() {
  driver_->Purge();
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

TEST_F(GCMDriverTest, Create) {
  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  EXPECT_FALSE(driver()->IsStarted());

  // Sign in. GCM is still not started.
  SignIn(kTestAccountID1);
  EXPECT_FALSE(driver()->IsStarted());
  EXPECT_FALSE(driver()->IsConnected());
  EXPECT_FALSE(gcm_connection_observer()->connected());

  // GCM will be started only after both sign-in and app handler being added.
  AddAppHandlers();
  EXPECT_TRUE(driver()->IsStarted());
  PumpIOLoop();
  EXPECT_TRUE(driver()->IsConnected());
  EXPECT_TRUE(gcm_connection_observer()->connected());
}

TEST_F(GCMDriverTest, CreateByFieldTrial) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("GCM", "Enabled"));

  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  EXPECT_FALSE(driver()->IsStarted());
  EXPECT_FALSE(driver()->IsConnected());
  EXPECT_FALSE(gcm_connection_observer()->connected());

  // GCM will be started after app handler is added.
  AddAppHandlers();
  EXPECT_TRUE(driver()->IsStarted());
  PumpIOLoop();
  EXPECT_TRUE(driver()->IsConnected());
  EXPECT_TRUE(gcm_connection_observer()->connected());
}

TEST_F(GCMDriverTest, Shutdown) {
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  EXPECT_FALSE(HasAppHandlers());

  AddAppHandlers();
  EXPECT_TRUE(HasAppHandlers());

  ShutdownDriver();
  EXPECT_FALSE(HasAppHandlers());
  EXPECT_FALSE(driver()->IsConnected());
  EXPECT_FALSE(gcm_connection_observer()->connected());
}

TEST_F(GCMDriverTest, SignInAndSignOutOnGCMEnabled) {
  // By default, GCM is enabled.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // GCMClient should be started after sign-in.
  SignIn(kTestAccountID1);
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // GCMClient should be checked out after sign-out.
  SignOut();
  EXPECT_EQ(FakeGCMClient::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, SignInAndSignOutOnGCMDisabled) {
  // By default, GCM is enabled.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // Disable GCM.
  driver()->Disable();

  // GCMClient should not be started after sign-in.
  SignIn(kTestAccountID1);
  EXPECT_EQ(FakeGCMClient::UNINITIALIZED, GetGCMClient()->status());

  // Check-out should still be performed after sign-out.
  SignOut();
  EXPECT_EQ(FakeGCMClient::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, SignOutAndThenSignIn) {
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // GCMClient should be started after sign-in.
  SignIn(kTestAccountID1);
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // GCMClient should be checked out after sign-out.
  SignOut();
  EXPECT_EQ(FakeGCMClient::CHECKED_OUT, GetGCMClient()->status());

  // Sign-in with a different account.
  SignIn(kTestAccountID2);

  // GCMClient should be started again.
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, DisableAndReenableGCM) {
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();
  SignIn(kTestAccountID1);

  // GCMClient should be started.
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // Disables the GCM.
  driver()->Disable();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_EQ(FakeGCMClient::STOPPED, GetGCMClient()->status());

  // Enables the GCM.
  driver()->Enable();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be started.
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // Disables the GCM.
  driver()->Disable();
  PumpIOLoop();
  PumpUILoop();

  // GCMClient should be stopped.
  EXPECT_EQ(FakeGCMClient::STOPPED, GetGCMClient()->status());

  // Sign out.
  SignOut();

  // GCMClient should be checked out.
  EXPECT_EQ(FakeGCMClient::CHECKED_OUT, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, StartOrStopGCMOnDemand) {
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  SignIn(kTestAccountID1);

  // GCMClient is not started.
  EXPECT_EQ(FakeGCMClient::UNINITIALIZED, GetGCMClient()->status());

  // GCMClient is started after an app handler has been added.
  driver()->AddAppHandler(kTestAppID1, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // Add another app handler.
  driver()->AddAppHandler(kTestAppID2, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // GCMClient remains active after one app handler is gone.
  driver()->RemoveAppHandler(kTestAppID1);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());

  // GCMClient should be stopped after the last app handler is gone.
  driver()->RemoveAppHandler(kTestAppID2);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(FakeGCMClient::STOPPED, GetGCMClient()->status());

  // GCMClient is restarted after an app handler has been added.
  driver()->AddAppHandler(kTestAppID2, gcm_app_handler());
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(FakeGCMClient::STARTED, GetGCMClient()->status());
}

TEST_F(GCMDriverTest, RegisterFailed) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");

  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // Registration fails when GCM is disabled.
  driver()->Disable();
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::GCM_DISABLED, registration_result());

  ClearResults();

  // Registration fails when the sign-in does not occur.
  driver()->Enable();
  AddAppHandlers();
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, registration_result());

  ClearResults();

  // Registration fails when the no app handler is added.
  RemoveAppHandlers();
  SignIn(kTestAccountID1);
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  EXPECT_TRUE(registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, registration_result());
}

TEST_F(GCMDriverTest, UnregisterFailed) {
  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // Unregistration fails when GCM is disabled.
  driver()->Disable();
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::GCM_DISABLED, unregistration_result());

  ClearResults();

  // Unregistration fails when the sign-in does not occur.
  driver()->Enable();
  AddAppHandlers();
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, unregistration_result());

  ClearResults();

  // Unregistration fails when the no app handler is added.
  RemoveAppHandlers();
  SignIn(kTestAccountID1);
  Unregister(kTestAppID1, GCMDriverTest::WAIT);
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, unregistration_result());
}

TEST_F(GCMDriverTest, SendFailed) {
  GCMClient::OutgoingMessage message;
  message.id = "1";
  message.data["key1"] = "value1";

  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // Sending fails when GCM is disabled.
  driver()->Disable();
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);
  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::GCM_DISABLED, send_result());

  ClearResults();

  // Sending fails when the sign-in does not occur.
  driver()->Enable();
  AddAppHandlers();
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);
  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, send_result());

  ClearResults();

  // Sending fails when the no app handler is added.
  RemoveAppHandlers();
  SignIn(kTestAccountID1);
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);
  EXPECT_TRUE(send_message_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, send_result());
}

TEST_F(GCMDriverTest, GCMClientNotReadyBeforeRegistration) {
  // Make GCMClient not ready initially.
  CreateDriver(FakeGCMClient::DELAY_START);
  SignIn(kTestAccountID1);
  AddAppHandlers();

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
  CreateDriver(FakeGCMClient::DELAY_START);
  SignIn(kTestAccountID1);
  AddAppHandlers();

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

  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();
  SignIn(kTestAccountID1);
}

TEST_F(GCMDriverFunctionalTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestAppID1, sender_ids, GCMDriverTest::WAIT);
  const std::string expected_registration_id =
      FakeGCMClient::GetRegistrationIdFromSenderIds(sender_ids);

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
      FakeGCMClient::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  ClearResults();

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
      FakeGCMClient::GetRegistrationIdFromSenderIds(sender_ids);

  EXPECT_EQ(expected_registration_id, registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  const std::string expected_registration_id2 =
      FakeGCMClient::GetRegistrationIdFromSenderIds(sender_ids);

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
  ClearResults();

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
  ClearResults();

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
  message.id = "1@ack";
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  Send(kTestAppID1, kUserID1, message, GCMDriverTest::WAIT);

  EXPECT_EQ(message.id, send_message_id());
  EXPECT_EQ(GCMClient::SUCCESS, send_result());

  gcm_app_handler()->WaitForNotification();
  EXPECT_EQ(message.id, gcm_app_handler()->acked_message_id());
  EXPECT_EQ(kTestAppID1, gcm_app_handler()->app_id());
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

// Tests a single instance of GCMDriver.
class GCMChannelStatusSyncerTest : public GCMDriverTest {
 public:
  GCMChannelStatusSyncerTest();
  virtual ~GCMChannelStatusSyncerTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;

  void CompleteGCMChannelStatusRequest(bool enabled, int poll_interval_seconds);
  bool CompareDelaySeconds(bool expected_delay_seconds,
                           bool actual_delay_seconds);

  GCMChannelStatusSyncer* syncer() {
    return driver()->gcm_channel_status_syncer_for_testing();
  }

 private:
  net::TestURLFetcherFactory url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMChannelStatusSyncerTest);
};

GCMChannelStatusSyncerTest::GCMChannelStatusSyncerTest() {
}

GCMChannelStatusSyncerTest::~GCMChannelStatusSyncerTest() {
}

void GCMChannelStatusSyncerTest::SetUp() {
  GCMDriverTest::SetUp();

  // Turn on all-user support.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("GCM", "Enabled"));
}

void GCMChannelStatusSyncerTest::CompleteGCMChannelStatusRequest(
    bool enabled, int poll_interval_seconds) {
  gcm_proto::ExperimentStatusResponse response_proto;
  response_proto.mutable_gcm_channel()->set_enabled(enabled);

  if (poll_interval_seconds)
    response_proto.set_poll_interval_seconds(poll_interval_seconds);

  std::string response_string;
  response_proto.SerializeToString(&response_string);

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

bool GCMChannelStatusSyncerTest::CompareDelaySeconds(
    bool expected_delay_seconds, bool actual_delay_seconds) {
  // Most of time, the actual delay should not be smaller than the expected
  // delay.
  if (actual_delay_seconds >= expected_delay_seconds)
    return true;
  // It is also OK that the actual delay is a bit smaller than the expected
  // delay in case that the test runs slowly.
  return expected_delay_seconds - actual_delay_seconds < 30;
}

TEST_F(GCMChannelStatusSyncerTest, DisableAndEnable) {
  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  EXPECT_FALSE(driver()->IsStarted());

  // By default, GCM is enabled.
  EXPECT_TRUE(driver()->gcm_enabled());
  EXPECT_TRUE(syncer()->gcm_enabled());

  // Remove delay such that the request could be executed immediately.
  syncer()->set_delay_removed_for_testing(true);

  // GCM will be started after app handler is added.
  AddAppHandlers();
  EXPECT_TRUE(driver()->IsStarted());

  // GCM is still enabled at this point.
  EXPECT_TRUE(driver()->gcm_enabled());
  EXPECT_TRUE(syncer()->gcm_enabled());

  // Wait until the GCM channel status request gets triggered.
  PumpUILoop();

  // Complete the request that disables the GCM.
  CompleteGCMChannelStatusRequest(false, 0);
  EXPECT_FALSE(driver()->gcm_enabled());
  EXPECT_FALSE(syncer()->gcm_enabled());
  EXPECT_FALSE(driver()->IsStarted());

  // Wait until next GCM channel status request gets triggered.
  PumpUILoop();

  // Complete the request that enables the GCM.
  CompleteGCMChannelStatusRequest(true, 0);
  EXPECT_TRUE(driver()->gcm_enabled());
  EXPECT_TRUE(syncer()->gcm_enabled());
  EXPECT_TRUE(driver()->IsStarted());
}

TEST_F(GCMChannelStatusSyncerTest, DisableAndRestart) {
  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  EXPECT_FALSE(driver()->IsStarted());

  // By default, GCM is enabled.
  EXPECT_TRUE(driver()->gcm_enabled());
  EXPECT_TRUE(syncer()->gcm_enabled());

  // Remove delay such that the request could be executed immediately.
  syncer()->set_delay_removed_for_testing(true);

  // GCM will be started after app handler is added.
  AddAppHandlers();
  EXPECT_TRUE(driver()->IsStarted());

  // GCM is still enabled at this point.
  EXPECT_TRUE(driver()->gcm_enabled());
  EXPECT_TRUE(syncer()->gcm_enabled());

  // Wait until the GCM channel status request gets triggered.
  PumpUILoop();

  // Complete the request that disables the GCM.
  CompleteGCMChannelStatusRequest(false, 0);
  EXPECT_FALSE(driver()->gcm_enabled());
  EXPECT_FALSE(syncer()->gcm_enabled());
  EXPECT_FALSE(driver()->IsStarted());

  // Simulate browser start by recreating GCMDriver.
  ShutdownDriver();
  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // GCM is still disabled.
  EXPECT_FALSE(driver()->gcm_enabled());
  EXPECT_FALSE(syncer()->gcm_enabled());
  EXPECT_FALSE(driver()->IsStarted());

  AddAppHandlers();
  EXPECT_FALSE(driver()->gcm_enabled());
  EXPECT_FALSE(syncer()->gcm_enabled());
  EXPECT_FALSE(driver()->IsStarted());
}

TEST_F(GCMChannelStatusSyncerTest, FirstTimePolling) {
  // Start GCM.
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // The 1st request should be triggered shortly without jittering.
  EXPECT_EQ(GCMChannelStatusSyncer::first_time_delay_seconds(),
            syncer()->current_request_delay_interval().InSeconds());
}

TEST_F(GCMChannelStatusSyncerTest, SubsequentPollingWithDefaultInterval) {
  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // Remove delay such that the request could be executed immediately.
  syncer()->set_delay_removed_for_testing(true);

  // Now GCM is started.
  AddAppHandlers();

  // Wait until the GCM channel status request gets triggered.
  PumpUILoop();

  // Keep delay such that we can find out the computed delay time.
  syncer()->set_delay_removed_for_testing(false);

  // Complete the request. The default interval is intact.
  CompleteGCMChannelStatusRequest(true, 0);

  // The next request should be scheduled at the expected default interval.
  int64 actual_delay_seconds =
      syncer()->current_request_delay_interval().InSeconds();
  int64 expected_delay_seconds =
      GCMChannelStatusRequest::default_poll_interval_seconds();
  EXPECT_TRUE(CompareDelaySeconds(expected_delay_seconds, actual_delay_seconds))
      << "expected delay: " << expected_delay_seconds
      << " actual delay: " << actual_delay_seconds;

  // Simulate browser start by recreating GCMDriver.
  ShutdownDriver();
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // After start-up, the request should still be scheduled at the expected
  // default interval.
  actual_delay_seconds =
      syncer()->current_request_delay_interval().InSeconds();
  EXPECT_TRUE(CompareDelaySeconds(expected_delay_seconds, actual_delay_seconds))
      << "expected delay: " << expected_delay_seconds
      << " actual delay: " << actual_delay_seconds;
}

TEST_F(GCMChannelStatusSyncerTest, SubsequentPollingWithUpdatedInterval) {
  // Create GCMDriver first. GCM is not started.
  CreateDriver(FakeGCMClient::NO_DELAY_START);

  // Remove delay such that the request could be executed immediately.
  syncer()->set_delay_removed_for_testing(true);

  // Now GCM is started.
  AddAppHandlers();

  // Wait until the GCM channel status request gets triggered.
  PumpUILoop();

  // Keep delay such that we can find out the computed delay time.
  syncer()->set_delay_removed_for_testing(false);

  // Complete the request. The interval is being changed.
  int new_poll_interval_seconds =
      GCMChannelStatusRequest::default_poll_interval_seconds() * 2;
  CompleteGCMChannelStatusRequest(true, new_poll_interval_seconds);

  // The next request should be scheduled at the expected updated interval.
  int64 actual_delay_seconds =
      syncer()->current_request_delay_interval().InSeconds();
  int64 expected_delay_seconds = new_poll_interval_seconds;
  EXPECT_TRUE(CompareDelaySeconds(expected_delay_seconds, actual_delay_seconds))
      << "expected delay: " << expected_delay_seconds
      << " actual delay: " << actual_delay_seconds;

  // Simulate browser start by recreating GCMDriver.
  ShutdownDriver();
  CreateDriver(FakeGCMClient::NO_DELAY_START);
  AddAppHandlers();

  // After start-up, the request should still be scheduled at the expected
  // updated interval.
  actual_delay_seconds =
      syncer()->current_request_delay_interval().InSeconds();
  EXPECT_TRUE(CompareDelaySeconds(expected_delay_seconds, actual_delay_seconds))
      << "expected delay: " << expected_delay_seconds
      << " actual delay: " << actual_delay_seconds;
}

}  // namespace gcm
