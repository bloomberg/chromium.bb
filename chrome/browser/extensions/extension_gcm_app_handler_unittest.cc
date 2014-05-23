// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_gcm_app_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/fake_gcm_app_handler.h"
#include "chrome/browser/services/gcm/fake_gcm_client.h"
#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"
#include "chrome/browser/services/gcm/fake_signin_manager.h"
#include "chrome/browser/services/gcm/gcm_driver.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/gcm/gcm_api.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

namespace {

const char kTestExtensionName[] = "FooBar";
const char kTestingUsername[] = "user1@example.com";

}  // namespace

// Helper class for asynchronous waiting.
class Waiter {
 public:
  Waiter() {}
  ~Waiter() {}

  // Waits until the asynchronous operation finishes.
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
        base::Bind(&Waiter::PumpIOLoopCompleted, base::Unretained(this)));
  }

  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

class FakeExtensionGCMAppHandler : public ExtensionGCMAppHandler {
 public:
  FakeExtensionGCMAppHandler(Profile* profile, Waiter* waiter)
      : ExtensionGCMAppHandler(profile),
        waiter_(waiter),
        unregistration_result_(gcm::GCMClient::UNKNOWN_ERROR) {
  }

  virtual ~FakeExtensionGCMAppHandler() {
  }

  virtual void OnMessage(
      const std::string& app_id,
      const gcm::GCMClient::IncomingMessage& message) OVERRIDE {
  }

  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE {
  }

  virtual void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) OVERRIDE {
  }

  virtual void OnUnregisterCompleted(const std::string& app_id,
                                     gcm::GCMClient::Result result) OVERRIDE {
    unregistration_result_ = result;
    waiter_->SignalCompleted();
  }

  gcm::GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

 private:
  Waiter* waiter_;
  gcm::GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeExtensionGCMAppHandler);
};

class ExtensionGCMAppHandlerTest : public testing::Test {
 public:
  static KeyedService* BuildGCMProfileService(
      content::BrowserContext* context) {
    return new gcm::GCMProfileService(
        Profile::FromBrowserContext(context),
        scoped_ptr<gcm::GCMClientFactory>(new gcm::FakeGCMClientFactory(
            gcm::FakeGCMClient::NO_DELAY_START,
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::UI),
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::IO))));
  }

  ExtensionGCMAppHandlerTest()
      : extension_service_(NULL),
        registration_result_(gcm::GCMClient::UNKNOWN_ERROR),
        unregistration_result_(gcm::GCMClient::UNKNOWN_ERROR) {
  }

  virtual ~ExtensionGCMAppHandlerTest() {
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

    // Create a new profile.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              gcm::FakeSigninManager::Build);
    profile_ = builder.Build();
    signin_manager_ = static_cast<gcm::FakeSigninManager*>(
        SigninManagerFactory::GetInstance()->GetForProfile(profile_.get()));

    // Create extension service in order to uninstall the extension.
    TestExtensionSystem* extension_system(
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile())));
    extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_ = extension_system->Get(profile())->extension_service();

    // Enable GCM such that tests could be run on all channels.
    profile()->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);

    // Create GCMProfileService that talks with fake GCMClient.
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &ExtensionGCMAppHandlerTest::BuildGCMProfileService);

    // Create a fake version of ExtensionGCMAppHandler.
    gcm_app_handler_.reset(new FakeExtensionGCMAppHandler(profile(), &waiter_));
  }

  virtual void TearDown() OVERRIDE {
#if defined(OS_CHROMEOS)
    test_user_manager_.reset();
#endif

    waiter_.PumpUILoop();
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

    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        path.AppendASCII(kTestExtensionName),
        Manifest::INVALID_LOCATION,
        manifest,
        Extension::NO_FLAGS,
        &error);
    EXPECT_TRUE(extension.get()) << error;
    EXPECT_TRUE(extension->HasAPIPermission(APIPermission::kGcm));

    return extension;
  }

  void LoadExtension(const Extension* extension) {
    extension_service_->AddExtension(extension);
  }

  void DisableExtension(const Extension* extension) {
    extension_service_->DisableExtension(
        extension->id(), Extension::DISABLE_USER_ACTION);
  }

  void EnableExtension(const Extension* extension) {
    extension_service_->EnableExtension(extension->id());
  }

  void UninstallExtension(const Extension* extension) {
    extension_service_->UninstallExtension(extension->id(), false, NULL);
  }

  void SignIn(const std::string& username) {
    signin_manager_->SignIn(username);
    waiter_.PumpIOLoop();
  }

  void SignOut() {
    signin_manager_->SignOut();
    waiter_.PumpIOLoop();
  }

  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids) {
    GetGCMDriver()->Register(
        app_id,
        sender_ids,
        base::Bind(&ExtensionGCMAppHandlerTest::RegisterCompleted,
                   base::Unretained(this)));
  }

  void RegisterCompleted(const std::string& registration_id,
                         gcm::GCMClient::Result result) {
    registration_result_ = result;
    waiter_.SignalCompleted();
  }

  gcm::GCMDriver* GetGCMDriver() const {
    return gcm::GCMProfileServiceFactory::GetForProfile(profile())->driver();
  }

  bool HasAppHandlers(const std::string& app_id) const {
    return GetGCMDriver()->app_handlers().count(app_id);
  }

  Profile* profile() const { return profile_.get(); }
  Waiter* waiter() { return &waiter_; }
  FakeExtensionGCMAppHandler* gcm_app_handler() const {
    return gcm_app_handler_.get();
  }
  gcm::GCMClient::Result registration_result() const {
    return registration_result_;
  }
  gcm::GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

 private:
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;  // Not owned.
  gcm::FakeSigninManager* signin_manager_;  // Not owned.

  // This is needed to create extension service under CrOS.
#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

  Waiter waiter_;
  scoped_ptr<FakeExtensionGCMAppHandler> gcm_app_handler_;
  gcm::GCMClient::Result registration_result_;
  gcm::GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionGCMAppHandlerTest);
};

TEST_F(ExtensionGCMAppHandlerTest, AddAndRemoveAppHandler) {
  scoped_refptr<Extension> extension(CreateExtension());

  // App handler is added when extension is loaded.
  LoadExtension(extension);
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler is removed when extension is unloaded.
  DisableExtension(extension);
  waiter()->PumpUILoop();
  EXPECT_FALSE(HasAppHandlers(extension->id()));

  // App handler is added when extension is reloaded.
  EnableExtension(extension);
  waiter()->PumpUILoop();
  EXPECT_TRUE(HasAppHandlers(extension->id()));

  // App handler is removed when extension is uninstalled.
  UninstallExtension(extension);
  waiter()->PumpUILoop();
  EXPECT_FALSE(HasAppHandlers(extension->id()));
}

TEST_F(ExtensionGCMAppHandlerTest, UnregisterOnExtensionUninstall) {
  scoped_refptr<Extension> extension(CreateExtension());
  LoadExtension(extension);

  // Sign-in is needed for registration.
  SignIn(kTestingUsername);

  // Kick off registration.
  std::vector<std::string> sender_ids;
  sender_ids.push_back("sender1");
  Register(extension->id(), sender_ids);
  waiter()->WaitUntilCompleted();
  EXPECT_EQ(gcm::GCMClient::SUCCESS, registration_result());

  // Add another app handler in order to prevent the GCM service from being
  // stopped when the extension is uninstalled. This is needed because otherwise
  // we are not able to receive the unregistration result.
  GetGCMDriver()->AddAppHandler("Foo", gcm_app_handler());

  // Unregistration should be triggered when the extension is uninstalled.
  UninstallExtension(extension);
  waiter()->WaitUntilCompleted();
  EXPECT_EQ(gcm::GCMClient::SUCCESS,
            gcm_app_handler()->unregistration_result());

  // Clean up.
  GetGCMDriver()->RemoveAppHandler("Foo");
}

}  // namespace extensions
