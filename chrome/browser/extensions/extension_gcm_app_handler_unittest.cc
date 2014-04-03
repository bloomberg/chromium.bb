// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/services/gcm/gcm_client_mock.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/services/gcm/gcm_profile_service_test_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#else
#include "components/signin/core/browser/signin_manager.h"
#endif

using namespace gcm;

namespace extensions {

namespace {

const char kTestExtensionName[] = "FooBar";
const char kTestingUsername[] = "user1@example.com";

}  // namespace

class FakeExtensionGCMAppHandler : public ExtensionGCMAppHandler {
 public:
  FakeExtensionGCMAppHandler(Profile* profile, Waiter* waiter)
      : ExtensionGCMAppHandler(profile),
        waiter_(waiter),
        unregistration_result_(GCMClient::UNKNOWN_ERROR) {
  }

  virtual ~FakeExtensionGCMAppHandler() {
  }

  virtual void OnMessage(
      const std::string& app_id,
      const GCMClient::IncomingMessage& message)OVERRIDE {
  }

  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE {
  }

  virtual void OnSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE {
  }

  virtual void OnUnregisterCompleted(const std::string& app_id,
                                     GCMClient::Result result) OVERRIDE {
    unregistration_result_ = result;
    waiter_->SignalCompleted();
  }

  GCMClient::Result unregistration_result() const {
    return unregistration_result_;
  }

 private:
  Waiter* waiter_;
  GCMClient::Result unregistration_result_;

  DISALLOW_COPY_AND_ASSIGN(FakeExtensionGCMAppHandler);
};

class ExtensionGCMAppHandlerTest : public testing::Test {
 public:
  static KeyedService* BuildGCMProfileService(
      content::BrowserContext* context) {
    return new GCMProfileService(static_cast<Profile*>(context));
  }

  ExtensionGCMAppHandlerTest()
      : extension_service_(NULL),
        registration_result_(GCMClient::UNKNOWN_ERROR),
        unregistration_result_(GCMClient::UNKNOWN_ERROR) {
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
    GCMProfileService* gcm_profile_service = static_cast<GCMProfileService*>(
        GCMProfileServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            &ExtensionGCMAppHandlerTest::BuildGCMProfileService));
    scoped_ptr<GCMClientFactory> gcm_client_factory(
        new FakeGCMClientFactory(GCMClientMock::NO_DELAY_LOADING));
    gcm_profile_service->Initialize(gcm_client_factory.Pass());

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
    GetGCMProfileService()->Register(
        app_id,
        sender_ids,
        base::Bind(&ExtensionGCMAppHandlerTest::RegisterCompleted,
                   base::Unretained(this)));
  }

  void RegisterCompleted(const std::string& registration_id,
                         GCMClient::Result result) {
    registration_result_ = result;
    waiter_.SignalCompleted();
  }

  GCMProfileService* GetGCMProfileService() const {
    return GCMProfileServiceFactory::GetForProfile(profile());
  }

  bool HasAppHandlers(const std::string& app_id) const {
    return GetGCMProfileService()->app_handlers_.count(app_id);
  }

  Profile* profile() const { return profile_.get(); }
  Waiter* waiter() { return &waiter_; }
  FakeExtensionGCMAppHandler* gcm_app_handler() const {
    return gcm_app_handler_.get();
  }
  GCMClient::Result registration_result() const { return registration_result_; }
  GCMClient::Result unregistration_result() const {
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
  GCMClient::Result registration_result_;
  GCMClient::Result unregistration_result_;

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
  EXPECT_EQ(GCMClient::SUCCESS, registration_result());

  // Unregistration should be triggered when the extension is uninstalled.
  UninstallExtension(extension);
  waiter()->WaitUntilCompleted();
  EXPECT_EQ(GCMClient::SUCCESS, gcm_app_handler()->unregistration_result());
}

}  // namespace extensions
