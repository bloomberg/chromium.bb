// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webdata/encryptor/encryptor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using namespace extensions;

namespace gcm {

const char kTestExtensionName[] = "FooBar";
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
  static BrowserContextKeyedService* BuildGCMProfileService(
      content::BrowserContext* profile) {
    return new GCMProfileService(
        static_cast<Profile*>(profile), gps_testing_delegate_);
  }

  GCMProfileServiceTest() : extension_service_(NULL) {
  }

  virtual ~GCMProfileServiceTest() {
  }

  // Overridden from test::Test:
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    // Make BrowserThread work in unittest.
    thread_bundle_.reset(new content::TestBrowserThreadBundle(
        content::TestBrowserThreadBundle::REAL_IO_THREAD));

    // This is needed to create extension service under CrOS.
#if defined(OS_CHROMEOS)
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif

    // Create extension service in order to uninstall the extension.
    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ = extension_system->Get(profile())->extension_service();

    // Mock a signed-in user.
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile());
    signin_manager->SetAuthenticatedUsername(kTestingUsername);

    // Encryptor ends up needing access to the keychain on OS X. So use the mock
    // keychain to prevent prompts.
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif

    // Mock a GCMClient.
    gcm_client_mock_.reset(new GCMClientMock());
    GCMClient::SetForTesting(gcm_client_mock_.get());

    // Mock a GCMEventRouter.
    gcm_event_router_mock_.reset(new GCMEventRouterMock(this));

    // Set |gps_testing_delegate_| for it to be picked up by
    // BuildGCMProfileService and pass it to GCMProfileService.
    gps_testing_delegate_ = this;

    // This will create GCMProfileService that causes check-in to be initiated.
    GCMProfileService::enable_gcm_for_testing_ = true;
    GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &GCMProfileServiceTest::BuildGCMProfileService);

    // Wait till the asynchronous check-in is done.
    WaitForCompleted();
  }

  virtual void TearDown() OVERRIDE {
    GCMClient::SetForTesting(NULL);

#if defined(OS_CHROMEOS)
    test_user_manager_.reset();
#endif

    extension_service_ = NULL;
    profile_.reset();
    base::RunLoop().RunUntilIdle();
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

  virtual void LoadingFromPersistentStoreFinished() OVERRIDE {
    SignalCompleted();
  }

  // Waits until the asynchrnous operation finishes.
  void WaitForCompleted() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  // Signals that the asynchrnous operation finishes.
  void SignalCompleted() {
    if (run_loop_ && run_loop_->running())
      run_loop_->Quit();
  }

  // Returns a barebones test extension.
  scoped_refptr<Extension> CreateExtension() {
#if defined(OS_WIN)
    base::FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
    base::FilePath path(FILE_PATH_LITERAL("/foo"));
#endif

    DictionaryValue manifest;
    manifest.SetString(manifest_keys::kVersion, "1.0.0.0");
    manifest.SetString(manifest_keys::kName, kTestExtensionName);
    ListValue* permission_list = new ListValue;
    permission_list->Append(Value::CreateStringValue("gcm"));
    manifest.Set(manifest_keys::kPermissions, permission_list);

    std::string error;
    scoped_refptr<Extension> extension =
        Extension::Create(path.AppendASCII(kTestExtensionName),
                          Manifest::INVALID_LOCATION,
                          manifest,
                          Extension::NO_FLAGS,
                          &error);
    EXPECT_TRUE(extension.get()) << error;

    extension_service_->AddExtension(extension.get());
    return extension;
  }

  Profile* profile() const { return profile_.get(); }

  GCMProfileService* GetGCMProfileService() const {
    return GCMProfileServiceFactory::GetForProfile(profile());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  ExtensionService* extension_service_;  // Not owned.
  scoped_ptr<GCMClientMock> gcm_client_mock_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<GCMEventRouterMock> gcm_event_router_mock_;
  GCMClient::CheckInInfo checkin_info_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

 private:
  static GCMProfileService::TestingDelegate* gps_testing_delegate_;

  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceTest);
};

GCMProfileService::TestingDelegate*
GCMProfileServiceTest::gps_testing_delegate_ = NULL;

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

TEST_F(GCMProfileServiceTest, CheckInFromPrefsStore) {
  // The first check-in should be successful.
  EXPECT_TRUE(checkin_info_.IsValid());
  GCMClient::CheckInInfo saved_checkin_info = checkin_info_;
  checkin_info_.Reset();

  gcm_client_mock_->set_checkin_failure_enabled(true);

  // Recreate GCMProfileService to test reading the check-in info from the
  // prefs store.
  GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &GCMProfileServiceTest::BuildGCMProfileService);

  EXPECT_EQ(saved_checkin_info.android_id, checkin_info_.android_id);
  EXPECT_EQ(saved_checkin_info.secret, checkin_info_.secret);
}

TEST_F(GCMProfileServiceTest, CheckOut) {
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMUserAccountID));
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMUserToken));

  GetGCMProfileService()->RemoveUser();

  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMUserAccountID));
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMUserToken));
}

class GCMProfileServiceRegisterTest : public GCMProfileServiceTest {
 public:
  GCMProfileServiceRegisterTest()
      : result_(GCMClient::SUCCESS),
        has_persisted_registration_info_(false) {
  }

  virtual ~GCMProfileServiceRegisterTest() {
  }

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids) {
    GetGCMProfileService()->Register(
        app_id,
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

  bool HasPersistedRegistrationInfo(const std::string& app_id) {
    StateStore* storage = ExtensionSystem::Get(profile())->state_store();
    if (!storage)
      return false;
    has_persisted_registration_info_ = false;
    storage->GetExtensionValue(
        app_id,
        GCMProfileService::GetPersistentRegisterKeyForTesting(),
        base::Bind(
            &GCMProfileServiceRegisterTest::ReadRegistrationInfoFinished,
            base::Unretained(this)));
    WaitForCompleted();
    return has_persisted_registration_info_;
  }

  void ReadRegistrationInfoFinished(scoped_ptr<base::Value> value) {
    has_persisted_registration_info_ = value.get() != NULL;
    SignalCompleted();
  }

 protected:
  std::string registration_id_;
  GCMClient::Result result_;
  bool has_persisted_registration_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMProfileServiceRegisterTest);
};

TEST_F(GCMProfileServiceRegisterTest, Register) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestingAppId, sender_ids);
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
  Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  // Calling regsiter 2nd time without waiting 1st one to finish will fail
  // immediately.
  sender_ids.push_back("sender2");
  Register(kTestingAppId, sender_ids);
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
  Register(kTestingAppId, sender_ids);

  WaitForCompleted();
  EXPECT_TRUE(registration_id_.empty());
  EXPECT_NE(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceRegisterTest, RegisterAgainWithSameSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  sender_ids.push_back("sender2");
  Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  WaitForCompleted();
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  registration_id_.clear();
  result_ = GCMClient::UNKNOWN_ERROR;

  // Calling register 2nd time with the same set of sender IDs but different
  // ordering will get back the same registration ID. There is no need to wait
  // since register simply returns the cached registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  Register(kTestingAppId, another_sender_ids);
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceRegisterTest, RegisterAgainWithDifferentSenderIDs) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  WaitForCompleted();
  EXPECT_EQ(expected_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);

  // Make sender IDs different.
  sender_ids.push_back("sender2");
  std::string expected_registration_id2 =
      gcm_client_mock_->GetRegistrationIdFromSenderIds(sender_ids);

  // Calling register 2nd time with the different sender IDs will get back a new
  // registration ID.
  Register(kTestingAppId, sender_ids);
  WaitForCompleted();
  EXPECT_EQ(expected_registration_id2, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

// http://crbug.com/326321
#if defined(OS_WIN)
#define MAYBE_RegisterFromStateStore DISABLED_RegisterFromStateStore
#else
#define MAYBE_RegisterFromStateStore RegisterFromStateStore
#endif
TEST_F(GCMProfileServiceRegisterTest, MAYBE_RegisterFromStateStore) {
  scoped_refptr<Extension> extension(CreateExtension());

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(extension->id(), sender_ids);

  WaitForCompleted();
  EXPECT_FALSE(registration_id_.empty());
  EXPECT_EQ(GCMClient::SUCCESS, result_);
  std::string old_registration_id = registration_id_;

  // Clears the results the would be set by the Register callback in preparation
  // to call register 2nd time.
  registration_id_.clear();
  result_ = GCMClient::UNKNOWN_ERROR;

  // Simulate start-up by recreating GCMProfileService.
  GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), &GCMProfileServiceTest::BuildGCMProfileService);

  // Simulate start-up by reloading extension.
  extension_service_->UnloadExtension(extension->id(),
                                      UnloadedExtensionInfo::REASON_TERMINATE);
  extension_service_->AddExtension(extension.get());

  // TODO(jianli): The waiting would be removed once we support delaying running
  // register operation until the persistent loading completes.
  WaitForCompleted();

  // This should read the registration info from the extension's state store.
  // There is no need to wait since register returns the registration ID being
  // read.
  Register(extension->id(), sender_ids);
  EXPECT_EQ(old_registration_id, registration_id_);
  EXPECT_EQ(GCMClient::SUCCESS, result_);
}

TEST_F(GCMProfileServiceRegisterTest, Unregister) {
  scoped_refptr<Extension> extension(CreateExtension());

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(extension->id(), sender_ids);

  WaitForCompleted();
  EXPECT_FALSE(registration_id_.empty());
  EXPECT_EQ(GCMClient::SUCCESS, result_);

  // The registration info should be cached.
  EXPECT_FALSE(GetGCMProfileService()->registration_info_map_.empty());

  // The registration info should be persisted.
  EXPECT_TRUE(HasPersistedRegistrationInfo(extension->id()));

  // Uninstall the extension.
  extension_service_->UninstallExtension(extension->id(), false, NULL);
  base::MessageLoop::current()->RunUntilIdle();

  // The cached registration info should be removed.
  EXPECT_TRUE(GetGCMProfileService()->registration_info_map_.empty());

  // The persisted registration info should be removed.
  EXPECT_FALSE(HasPersistedRegistrationInfo(extension->id()));
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
