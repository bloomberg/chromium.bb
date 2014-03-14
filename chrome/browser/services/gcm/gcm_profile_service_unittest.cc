// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/state_store.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/services/gcm/gcm_client_factory.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/signin_manager_base.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/encryptor/os_crypt.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
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

namespace {

const char kTestExtensionName[] = "FooBar";
const char kTestingUsername[] = "user1@example.com";
const char kTestingUsername2[] = "user2@example.com";
const char kTestingUsername3[] = "user3@example.com";
const char kTestingAppId[] = "TestApp1";
const char kTestingAppId2[] = "TestApp2";
const char kUserId[] = "user1";
const char kUserId2[] = "user2";

std::vector<std::string> ToSenderList(const std::string& sender_ids) {
  std::vector<std::string> senders;
  base::StringTokenizer tokenizer(sender_ids, ",");
  while (tokenizer.GetNext())
    senders.push_back(tokenizer.token());
  return senders;
}

// Helper class for asynchrnous waiting.
class Waiter {
 public:
  Waiter() {}
  virtual ~Waiter() {}

  // Waits until the asynchrnous operation finishes.
  void WaitUntilCompleted() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  // Signals that the asynchronous operation finishes.
  void SignalCompleted() {
    if (run_loop_ && run_loop_->running())
      run_loop_->Quit();
  }

  // Runs until UI loop becomes idle.
  void PumpUILoop() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  // Runs until IO loop becomes idle.
  void PumpIOLoop() {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Waiter::OnIOLoopPump, base::Unretained(this)));

    WaitUntilCompleted();
  }

 private:
  void PumpIOLoopCompleted() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    SignalCompleted();
  }

  void OnIOLoopPump() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Waiter::OnIOLoopPumpCompleted, base::Unretained(this)));
  }

  void OnIOLoopPumpCompleted() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&Waiter::PumpIOLoopCompleted,
                   base::Unretained(this)));
  }

  scoped_ptr<base::RunLoop> run_loop_;
};

class FakeSigninManager : public SigninManagerBase {
 public:
  explicit FakeSigninManager(Profile* profile) {
    Initialize(profile, NULL);
  }

  virtual ~FakeSigninManager() {
  }

  void SignIn(const std::string& username) {
    SetAuthenticatedUsername(username);
    FOR_EACH_OBSERVER(Observer,
                      observer_list_,
                      GoogleSigninSucceeded(username, std::string()));
  }

  void SignOut() {
    std::string username = GetAuthenticatedUsername();
    clear_authenticated_username();
    profile()->GetPrefs()->ClearPref(prefs::kGoogleServicesUsername);
    FOR_EACH_OBSERVER(Observer, observer_list_, GoogleSignedOut(username));
  }
};

}  // namespace

class FakeGCMEventRouter : public GCMEventRouter {
 public:
  enum Event {
    NO_EVENT,
    MESSAGE_EVENT,
    MESSAGES_DELETED_EVENT,
    SEND_ERROR_EVENT
  };

  explicit FakeGCMEventRouter(Waiter* waiter)
      : waiter_(waiter), received_event_(NO_EVENT) {}

  virtual ~FakeGCMEventRouter() {
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

class FakeGCMClientFactory : public GCMClientFactory {
 public:
  FakeGCMClientFactory(
      GCMClientMock::LoadingDelay gcm_client_loading_delay,
      GCMClientMock::ErrorSimulation gcm_client_error_simulation)
      : gcm_client_loading_delay_(gcm_client_loading_delay),
        gcm_client_error_simulation_(gcm_client_error_simulation),
        gcm_client_(NULL) {
  }

  virtual ~FakeGCMClientFactory() {
  }

  virtual scoped_ptr<GCMClient> BuildInstance() OVERRIDE {
    gcm_client_ = new GCMClientMock(gcm_client_loading_delay_,
                                    gcm_client_error_simulation_);
    return scoped_ptr<GCMClient>(gcm_client_);
  }

  GCMClientMock* gcm_client() const { return gcm_client_; }

 private:
  GCMClientMock::LoadingDelay gcm_client_loading_delay_;
  GCMClientMock::ErrorSimulation gcm_client_error_simulation_;
  GCMClientMock* gcm_client_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMClientFactory);
};

class GCMProfileServiceTestConsumer : public GCMProfileService::TestingDelegate{
 public:
  static KeyedService* BuildFakeSigninManager(
      content::BrowserContext* context) {
    return new FakeSigninManager(static_cast<Profile*>(context));
  }

  static KeyedService* BuildGCMProfileService(
      content::BrowserContext* context) {
    return new GCMProfileService(static_cast<Profile*>(context));
  }

  explicit GCMProfileServiceTestConsumer(Waiter* waiter)
      : waiter_(waiter),
        extension_service_(NULL),
        signin_manager_(NULL),
        gcm_client_loading_delay_(GCMClientMock::NO_DELAY_LOADING),
        gcm_client_error_simulation_(GCMClientMock::ALWAYS_SUCCEED),
        registration_result_(GCMClient::SUCCESS),
        has_persisted_registration_info_(false),
        send_result_(GCMClient::SUCCESS) {
    // Create a new profile.
    profile_.reset(new TestingProfile);

    // Use a fake version of SigninManager.
    signin_manager_ = static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &GCMProfileServiceTestConsumer::BuildFakeSigninManager));

    // Create extension service in order to uninstall the extension.
    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ = extension_system->Get(profile())->extension_service();

    // EventRouter is needed for GcmJsEventRouter.
    if (!extension_system->event_router()) {
      extension_system->SetEventRouter(scoped_ptr<EventRouter>(
          new EventRouter(profile(), ExtensionPrefs::Get(profile()))));
    }

    // Enable GCM such that tests could be run on all channels.
    profile()->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);

    // Mock a GCMEventRouter.
    gcm_event_router_.reset(new FakeGCMEventRouter(waiter_));
  }

  virtual ~GCMProfileServiceTestConsumer() {
  }

  // Returns a barebones test extension.
  scoped_refptr<Extension> CreateExtension() {
#if defined(OS_WIN)
    base::FilePath path(FILE_PATH_LITERAL("c:\\foo"));
#elif defined(OS_POSIX)
    base::FilePath path(FILE_PATH_LITERAL("/foo"));
#endif

    base::DictionaryValue manifest;
    manifest.SetString(manifest_keys::kVersion, "1.0.0.0");
    manifest.SetString(manifest_keys::kName, kTestExtensionName);
    base::ListValue* permission_list = new base::ListValue;
    permission_list->Append(base::Value::CreateStringValue("gcm"));
    manifest.Set(manifest_keys::kPermissions, permission_list);

    // TODO(jianli): Once the GCM API enters stable, remove |channel|.
    ScopedCurrentChannel channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
    std::string error;
    scoped_refptr<Extension> extension =
        Extension::Create(path.AppendASCII(kTestExtensionName),
                          Manifest::INVALID_LOCATION,
                          manifest,
                          Extension::NO_FLAGS,
                          &error);
    EXPECT_TRUE(extension.get()) << error;
    EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kGcm));

    extension_service_->AddExtension(extension.get());
    return extension;
  }

  void UninstallExtension(const extensions::Extension* extension) {
    extension_service_->UninstallExtension(extension->id(), false, NULL);
  }

  void ReloadExtension(const extensions::Extension* extension) {
    extension_service_->UnloadExtension(
        extension->id(), UnloadedExtensionInfo::REASON_TERMINATE);
    extension_service_->AddExtension(extension);
  }

  void CreateGCMProfileServiceInstance() {
    GCMProfileService* gcm_profile_service = static_cast<GCMProfileService*>(
        GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &GCMProfileServiceTestConsumer::BuildGCMProfileService));
    gcm_profile_service->set_testing_delegate(this);
    scoped_ptr<GCMClientFactory> gcm_client_factory(
        new FakeGCMClientFactory(gcm_client_loading_delay_,
                                 gcm_client_error_simulation_));
    gcm_profile_service->Initialize(gcm_client_factory.Pass());
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

  bool HasPersistedRegistrationInfo(const std::string& app_id) {
    StateStore* storage = ExtensionSystem::Get(profile())->state_store();
    if (!storage)
      return false;
    has_persisted_registration_info_ = false;
    storage->GetExtensionValue(
        app_id,
        GCMProfileService::GetPersistentRegisterKeyForTesting(),
        base::Bind(
            &GCMProfileServiceTestConsumer::ReadRegistrationInfoFinished,
            base::Unretained(this)));
    waiter_->WaitUntilCompleted();
    return has_persisted_registration_info_;
  }

  void ReadRegistrationInfoFinished(scoped_ptr<base::Value> value) {
    has_persisted_registration_info_ = value.get() != NULL;
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

  bool ExistsCachedRegistrationInfo() const {
    return !GetGCMProfileService()->registration_info_map_.empty();
  }

  Profile* profile() const { return profile_.get(); }
  FakeGCMEventRouter* gcm_event_router() const {
    return gcm_event_router_.get();
  }

  void set_gcm_client_loading_delay(GCMClientMock::LoadingDelay delay) {
    gcm_client_loading_delay_ = delay;
  }
  void set_gcm_client_error_simulation(GCMClientMock::ErrorSimulation error) {
    gcm_client_error_simulation_ = error;
  }

  const std::string& registration_id() const { return registration_id_; }
  GCMClient::Result registration_result() const { return registration_result_; }
  const std::string& send_message_id() const { return send_message_id_; }
  GCMClient::Result send_result() const { return send_result_; }

  void clear_registration_result() {
    registration_id_.clear();
    registration_result_ = GCMClient::UNKNOWN_ERROR;
  }
  void clear_send_result() {
    send_message_id_.clear();
    send_result_ = GCMClient::UNKNOWN_ERROR;
  }

 private:
  // Overridden from GCMProfileService::TestingDelegate:
  virtual GCMEventRouter* GetEventRouter() const OVERRIDE {
    return gcm_event_router_.get();
  }

  Waiter* waiter_;  // Not owned.
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;  // Not owned.
  FakeSigninManager* signin_manager_;  // Not owned.
  scoped_ptr<FakeGCMEventRouter> gcm_event_router_;

  GCMClientMock::LoadingDelay gcm_client_loading_delay_;
  GCMClientMock::ErrorSimulation gcm_client_error_simulation_;

  std::string registration_id_;
  GCMClient::Result registration_result_;
  bool has_persisted_registration_info_;

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

    // This is needed to create extension service under CrOS.
#if defined(OS_CHROMEOS)
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif

    // OSCrypt ends up needing access to the keychain on OS X. So use the mock
    // keychain to prevent prompts.
#if defined(OS_MACOSX)
    OSCrypt::UseMockKeychain(true);
#endif

    // Create a main profile consumer.
    consumer_.reset(new GCMProfileServiceTestConsumer(&waiter_));
  }

  virtual void TearDown() OVERRIDE {
#if defined(OS_CHROMEOS)
    test_user_manager_.reset();
#endif

    consumer_.reset();
    base::RunLoop().RunUntilIdle();
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
#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

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

TEST_F(GCMProfileServiceSingleProfileTest, DoubleRegister) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  std::string expected_registration_id =
      GCMClientMock::GetRegistrationIdFromSenderIds(sender_ids);

  // Calling regsiter 2nd time without waiting 1st one to finish will fail
  // immediately.
  sender_ids.push_back("sender2");
  consumer()->Register(kTestingAppId, sender_ids);
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::ASYNC_OPERATION_PENDING,
            consumer()->registration_result());

  // The 1st register is still doing fine.
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
  // ordering will get back the same registration ID. There is no need to wait
  // since register simply returns the cached registration ID.
  std::vector<std::string> another_sender_ids;
  another_sender_ids.push_back("sender2");
  another_sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, another_sender_ids);
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

TEST_F(GCMProfileServiceSingleProfileTest, ReadRegistrationFromStateStore) {
  scoped_refptr<Extension> extension(consumer()->CreateExtension());

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(extension->id(), sender_ids);

  WaitUntilCompleted();
  EXPECT_FALSE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
  std::string old_registration_id = consumer()->registration_id();

  // Clears the results that would be set by the Register callback in
  // preparation to call register 2nd time.
  consumer()->clear_registration_result();

  // Register should not reach the server. Forcing GCMClient server error should
  // help catch this.
  consumer()->set_gcm_client_error_simulation(GCMClientMock::FORCE_ERROR);

  // Simulate start-up by recreating GCMProfileService.
  consumer()->CreateGCMProfileServiceInstance();

  // Simulate start-up by reloading extension.
  consumer()->ReloadExtension(extension);

  // This should read the registration info from the extension's state store.
  consumer()->Register(extension->id(), sender_ids);
  PumpIOLoop();
  PumpUILoop();
  EXPECT_EQ(old_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest,
       GCMClientReadyAfterReadingRegistration) {
  scoped_refptr<Extension> extension(consumer()->CreateExtension());

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(extension->id(), sender_ids);

  WaitUntilCompleted();
  EXPECT_FALSE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
  std::string old_registration_id = consumer()->registration_id();

  // Clears the results that would be set by the Register callback in
  // preparation to call register 2nd time.
  consumer()->clear_registration_result();

  // Simulate start-up by recreating GCMProfileService.
  consumer()->set_gcm_client_loading_delay(GCMClientMock::DELAY_LOADING);
  consumer()->CreateGCMProfileServiceInstance();

  // Simulate start-up by reloading extension.
  consumer()->ReloadExtension(extension);

  // Read the registration info from the extension's state store.
  // This would hold up because GCMClient is in loading state.
  consumer()->Register(extension->id(), sender_ids);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::UNKNOWN_ERROR, consumer()->registration_result());

  // Register operation will be invoked after GCMClient becomes ready.
  consumer()->GetGCMClient()->PerformDelayedLoading();
  WaitUntilCompleted();
  EXPECT_EQ(old_registration_id, consumer()->registration_id());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());
}

TEST_F(GCMProfileServiceSingleProfileTest,
       PersistedRegistrationInfoRemoveAfterSignOut) {
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(kTestingAppId, sender_ids);
  WaitUntilCompleted();

  // The app id and registration info should be persisted.
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMRegisteredAppIDs));
  EXPECT_TRUE(consumer()->HasPersistedRegistrationInfo(kTestingAppId));

  consumer()->SignOut();
  PumpUILoop();

  // The app id and persisted registration info should be removed.
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kGCMRegisteredAppIDs));
  EXPECT_FALSE(consumer()->HasPersistedRegistrationInfo(kTestingAppId));
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

TEST_F(GCMProfileServiceSingleProfileTest, Unregister) {
  scoped_refptr<Extension> extension(consumer()->CreateExtension());

  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  consumer()->Register(extension->id(), sender_ids);

  WaitUntilCompleted();
  EXPECT_FALSE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::SUCCESS, consumer()->registration_result());

  // The registration info should be cached.
  EXPECT_TRUE(consumer()->ExistsCachedRegistrationInfo());

  // The registration info should be persisted.
  EXPECT_TRUE(consumer()->HasPersistedRegistrationInfo(extension->id()));

  // Uninstall the extension.
  consumer()->UninstallExtension(extension);
  base::MessageLoop::current()->RunUntilIdle();

  // The cached registration info should be removed.
  EXPECT_FALSE(consumer()->ExistsCachedRegistrationInfo());

  // The persisted registration info should be removed.
  EXPECT_FALSE(consumer()->HasPersistedRegistrationInfo(extension->id()));
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
  EXPECT_EQ(FakeGCMEventRouter::SEND_ERROR_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
  EXPECT_EQ(consumer()->send_message_id(),
            consumer()->gcm_event_router()->send_error_message_id());
  EXPECT_NE(GCMClient::SUCCESS,
            consumer()->gcm_event_router()->send_error_result());
  EXPECT_EQ(message.data, consumer()->gcm_event_router()->send_error_data());
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
  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_event_router()->message().data);
  EXPECT_TRUE(consumer()->gcm_event_router()->message().collapse_key.empty());
  EXPECT_EQ(message.sender_id,
            consumer()->gcm_event_router()->message().sender_id);
}

TEST_F(GCMProfileServiceSingleProfileTest,
       MessageNotReceivedFromNotRegisteredSender) {
  // Explicitly not registering the sender2 here, so that message gets dropped.
  consumer()->Register(kTestingAppId, ToSenderList("sender1"));
  WaitUntilCompleted();
  GCMClient::IncomingMessage message;
  message.data["key1"] = "value1";
  message.data["key2"] = "value2";
  message.sender_id = "sender2";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  PumpUILoop();
  EXPECT_EQ(FakeGCMEventRouter::NO_EVENT,
            consumer()->gcm_event_router()->received_event());
  consumer()->gcm_event_router()->clear_results();

  // Register for sender2 and try to receive the message again, which should
  // work with no problems.
  consumer()->Register(kTestingAppId, ToSenderList("sender1,sender2"));
  WaitUntilCompleted();
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  consumer()->gcm_event_router()->clear_results();

  // Making sure that sender1 can receive the message as well.
  message.sender_id = "sender1";
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  consumer()->gcm_event_router()->clear_results();

  // Register for sender1 only and make sure it is not possible  to receive the
  // message again from from sender1.
  consumer()->Register(kTestingAppId, ToSenderList("sender2"));
  WaitUntilCompleted();
  consumer()->GetGCMClient()->ReceiveMessage(kTestingAppId, message);
  PumpUILoop();
  EXPECT_EQ(FakeGCMEventRouter::NO_EVENT,
            consumer()->gcm_event_router()->received_event());
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
  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_event_router()->message().data);
  EXPECT_EQ(message.collapse_key,
            consumer()->gcm_event_router()->message().collapse_key);
}

TEST_F(GCMProfileServiceSingleProfileTest, MessagesDeleted) {
  consumer()->GetGCMClient()->DeleteMessages(kTestingAppId);
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMEventRouter::MESSAGES_DELETED_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
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

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
  EXPECT_TRUE(message.data == consumer()->gcm_event_router()->message().data);
  EXPECT_EQ("sender", consumer()->gcm_event_router()->message().sender_id);

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_event_router()->app_id());
  EXPECT_TRUE(message2.data == consumer2()->gcm_event_router()->message().data);
  EXPECT_EQ("sender", consumer2()->gcm_event_router()->message().sender_id);

  // Trigger another incoming message for a different app in another profile.
  GCMClient::IncomingMessage message3;
  message3.data["bar1"] = "foo1";
  message3.data["bar2"] = "foo2";
  message3.sender_id = "sender2";
  consumer2()->GetGCMClient()->ReceiveMessage(kTestingAppId2, message3);

  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_event_router()->app_id());
  EXPECT_TRUE(message3.data == consumer2()->gcm_event_router()->message().data);
  EXPECT_EQ("sender2", consumer2()->gcm_event_router()->message().sender_id);
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

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer()->gcm_event_router()->app_id());
  EXPECT_TRUE(
      in_message.data == consumer()->gcm_event_router()->message().data);

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

  consumer2()->gcm_event_router()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_event_router()->app_id());
  EXPECT_TRUE(
      in_message2.data == consumer2()->gcm_event_router()->message().data);

  consumer2()->gcm_event_router()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_event_router()->app_id());
  EXPECT_TRUE(
      in_message3.data == consumer2()->gcm_event_router()->message().data);

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
  consumer()->gcm_event_router()->clear_results();
  consumer()->Register(kTestingAppId, sender_ids);
  EXPECT_TRUE(consumer()->registration_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->registration_result());

  consumer()->gcm_event_router()->clear_results();
  consumer()->Send(kTestingAppId2, kUserId2, out_message3);
  EXPECT_TRUE(consumer()->send_message_id().empty());
  EXPECT_EQ(GCMClient::NOT_SIGNED_IN, consumer()->send_result());

  // Deleted messages event will go through for another signed-in profile.
  consumer2()->GetGCMClient()->DeleteMessages(kTestingAppId2);

  consumer2()->gcm_event_router()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMEventRouter::MESSAGES_DELETED_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId2, consumer2()->gcm_event_router()->app_id());

  // Send error event will go through for another signed-in profile.
  GCMClient::OutgoingMessage out_message4;
  out_message4.id = "1@error";
  out_message4.data["out4"] = "out_data4";
  consumer2()->Send(kTestingAppId, kUserId, out_message4);

  WaitUntilCompleted();
  EXPECT_EQ(consumer2()->send_message_id(), out_message4.id);
  EXPECT_EQ(GCMClient::SUCCESS, consumer2()->send_result());

  consumer2()->gcm_event_router()->clear_results();
  WaitUntilCompleted();
  EXPECT_EQ(FakeGCMEventRouter::SEND_ERROR_EVENT,
            consumer2()->gcm_event_router()->received_event());
  EXPECT_EQ(kTestingAppId, consumer2()->gcm_event_router()->app_id());
  EXPECT_EQ(out_message4.id,
            consumer2()->gcm_event_router()->send_error_message_id());
  EXPECT_NE(GCMClient::SUCCESS,
            consumer2()->gcm_event_router()->send_error_result());
  EXPECT_EQ(out_message4.data,
            consumer2()->gcm_event_router()->send_error_data());

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

  consumer()->gcm_event_router()->clear_results();
  WaitUntilCompleted();

  EXPECT_EQ(FakeGCMEventRouter::MESSAGE_EVENT,
            consumer()->gcm_event_router()->received_event());
  EXPECT_TRUE(
      in_message5.data == consumer()->gcm_event_router()->message().data);
}

}  // namespace gcm
