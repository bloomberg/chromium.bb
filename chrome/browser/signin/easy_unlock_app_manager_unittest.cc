// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_app_manager.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/api/screenlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/proximity_auth/switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "grit/browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace easy_unlock_private_api = extensions::api::easy_unlock_private;
namespace screenlock_private_api = extensions::api::screenlock_private;
namespace app_runtime_api = extensions::api::app_runtime;

namespace {

// Sets |*value| to true, also verifying that the value was not previously set.
// Used in tests for verifying that a callback was called.
void VerifyFalseAndSetToTrue(bool* value) {
  EXPECT_FALSE(*value);
  *value = true;
}

// A ProcessManager that doesn't create background host pages.
class TestProcessManager : public extensions::ProcessManager {
 public:
  explicit TestProcessManager(content::BrowserContext* context)
      : extensions::ProcessManager(
            context,
            context,
            extensions::ExtensionRegistry::Get(context)) {}
  ~TestProcessManager() override {}

  // ProcessManager overrides:
  bool CreateBackgroundHost(const extensions::Extension* extension,
                            const GURL& url) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProcessManager);
};

std::unique_ptr<KeyedService> CreateTestProcessManager(
    content::BrowserContext* context) {
  return base::WrapUnique(new TestProcessManager(context));
}

std::unique_ptr<KeyedService> CreateScreenlockPrivateEventRouter(
    content::BrowserContext* context) {
  return base::WrapUnique(
      new extensions::ScreenlockPrivateEventRouter(context));
}

// Observes extension registry for unload and load events (in that order) of an
// extension with the provided extension id.
// Used to determine if an extension was reloaded.
class ExtensionReloadTracker : public extensions::ExtensionRegistryObserver {
 public:
  ExtensionReloadTracker(Profile* profile, const std::string& extension_id)
      : profile_(profile),
        extension_id_(extension_id),
        unloaded_(false),
        loaded_(false) {
    extensions::ExtensionRegistry::Get(profile)->AddObserver(this);
  }

  ~ExtensionReloadTracker() override {
    extensions::ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  }

  // extension::ExtensionRegistryObserver implementation:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    ASSERT_FALSE(loaded_);
    ASSERT_EQ(extension_id_, extension->id());
    loaded_ = true;
  }

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override {
    ASSERT_FALSE(unloaded_);
    ASSERT_EQ(extension_id_, extension->id());
    unloaded_ = true;
  }

  // Whether the extensino was unloaded and loaded during |this| lifetime.
  bool HasReloaded() const { return loaded_ && unloaded_; }

 private:
  Profile* profile_;
  std::string extension_id_;
  bool unloaded_;
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionReloadTracker);
};

// Consumes events dispatched from test event router.
class EasyUnlockAppEventConsumer {
 public:
  explicit EasyUnlockAppEventConsumer(Profile* profile)
      : user_updated_count_(0u),
        auth_attempted_count_(0u),
        app_launched_count_(0u) {}

  ~EasyUnlockAppEventConsumer() {}

  // Processes event for test event router.
  // It returns whether the event is expected to be dispatched during tests and
  // whether it's well formed.
  bool ConsumeEvent(const std::string& event_name, base::ListValue* args) {
    if (event_name == easy_unlock_private_api::OnUserInfoUpdated::kEventName)
      return ConsumeUserInfoUpdated(args);

    if (event_name == screenlock_private_api::OnAuthAttempted::kEventName)
      return ConsumeAuthAttempted(args);

    if (event_name == app_runtime_api::OnLaunched::kEventName)
      return ConsumeLaunched(args);

    LOG(ERROR) << "Unexpected event: " << event_name;
    return false;
  }

  // Information about encountered events:
  size_t user_updated_count() const { return user_updated_count_; }
  size_t auth_attempted_count() const { return auth_attempted_count_; }
  size_t app_launched_count() const { return app_launched_count_; }

  // The data carried by the last UserInfoUpdated event:
  std::string user_id() const { return user_id_; }
  bool user_logged_in() const { return user_logged_in_; }
  bool user_data_ready() const { return user_data_ready_; }

 private:
  // Processes easyUnlockPrivate.onUserInfoUpdated event.
  bool ConsumeUserInfoUpdated(base::ListValue* args) {
    if (!args) {
      LOG(ERROR) << "No argument list for onUserInfoUpdated event.";
      return false;
    }

    if (args->GetSize() != 1u) {
      LOG(ERROR) << "Invalid argument list size for onUserInfoUpdated event: "
                 << args->GetSize() << " expected: " << 1u;
      return false;
    }

    base::DictionaryValue* user_info;
    if (!args->GetDictionary(0u, &user_info) || !user_info) {
      LOG(ERROR) << "Unabled to get event argument as dictionary for "
                 << "onUserInfoUpdated event.";
      return false;
    }

    EXPECT_TRUE(user_info->GetString("userId", &user_id_));
    EXPECT_TRUE(user_info->GetBoolean("loggedIn", &user_logged_in_));
    EXPECT_TRUE(user_info->GetBoolean("dataReady", &user_data_ready_));

    ++user_updated_count_;
    return true;
  }

  // Processes screenlockPrivate.onAuthAttempted event.
  bool ConsumeAuthAttempted(base::ListValue* args) {
    if (!args) {
      LOG(ERROR) << "No argument list for onAuthAttempted event";
      return false;
    }

    if (args->GetSize() != 2u) {
      LOG(ERROR) << "Invalid argument list size for onAuthAttempted event: "
                 << args->GetSize() << " expected: " << 2u;
      return false;
    }

    std::string auth_type;
    if (!args->GetString(0u, &auth_type)) {
      LOG(ERROR) << "Unable to get first argument as string for "
                 << "onAuthAttempted event.";
      return false;
    }

    EXPECT_EQ("userClick", auth_type);
    ++auth_attempted_count_;
    return true;
  }

  // Processes app.runtime.onLaunched event.
  bool ConsumeLaunched(base::ListValue* args) {
    ++app_launched_count_;
    return true;
  }

  size_t user_updated_count_;
  size_t auth_attempted_count_;
  size_t app_launched_count_;

  std::string user_id_;
  bool user_logged_in_;
  bool user_data_ready_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAppEventConsumer);
};

// Event router injected into extension system for the tests. It redirects
// events to EasyUnlockAppEventConsumer.
class TestEventRouter : public extensions::EventRouter {
 public:
  TestEventRouter(Profile* profile, extensions::ExtensionPrefs* extension_prefs)
      : extensions::EventRouter(profile, extension_prefs) {}

  ~TestEventRouter() override {}

  // extensions::EventRouter implementation:
  void BroadcastEvent(std::unique_ptr<extensions::Event> event) override {
    ASSERT_EQ(screenlock_private_api::OnAuthAttempted::kEventName,
              event->event_name);
    EXPECT_TRUE(event_consumer_->ConsumeEvent(event->event_name,
                                              event->event_args.get()));
  }

  void DispatchEventToExtension(
      const std::string& extension_id,
      std::unique_ptr<extensions::Event> event) override {
    ASSERT_EQ(extension_misc::kEasyUnlockAppId, extension_id);
    EXPECT_TRUE(event_consumer_->ConsumeEvent(event->event_name,
                                              event->event_args.get()));
  }

  void set_event_consumer(EasyUnlockAppEventConsumer* event_consumer) {
    event_consumer_ = event_consumer;
  }

 private:
  EasyUnlockAppEventConsumer* event_consumer_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouter);
};

// TestEventRouter factory function
std::unique_ptr<KeyedService> TestEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return base::WrapUnique(
      new TestEventRouter(static_cast<Profile*>(context),
                          extensions::ExtensionPrefs::Get(context)));
}

class EasyUnlockAppManagerTest : public testing::Test {
 public:
  EasyUnlockAppManagerTest()
      : event_consumer_(&profile_),
        command_line_(base::CommandLine::NO_PROGRAM) {}
  ~EasyUnlockAppManagerTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        proximity_auth::switches::kForceLoadEasyUnlockAppInTests);
    extensions::ExtensionSystem* extension_system = SetUpExtensionSystem();
    app_manager_ = EasyUnlockAppManager::Create(
        extension_system, IDR_EASY_UNLOCK_MANIFEST, GetAppPath());
  }

 protected:
  void SetExtensionSystemReady() {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    test_extension_system->SetReady();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath GetAppPath() {
    return extensions::ExtensionPrefs::Get(&profile_)
        ->install_directory()
        .AppendASCII("easy_unlock");
  }

 private:
  // Initializes test extension system.
  extensions::ExtensionSystem* SetUpExtensionSystem() {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    extension_service_ = test_extension_system->CreateExtensionService(
        &command_line_, base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);

    extensions::ProcessManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, &CreateTestProcessManager);
    extensions::ScreenlockPrivateEventRouter::GetFactoryInstance()
        ->SetTestingFactory(&profile_, &CreateScreenlockPrivateEventRouter);

    event_router_ = static_cast<TestEventRouter*>(
        extensions::EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, &TestEventRouterFactoryFunction));
    event_router_->set_event_consumer(&event_consumer_);

    extension_service_->component_loader()->
        set_ignore_whitelist_for_testing(true);

    return test_extension_system;
  }

 protected:
  std::unique_ptr<EasyUnlockAppManager> app_manager_;

  // Needed by extension system.
  content::TestBrowserThreadBundle thread_bundle_;

#if defined(OS_CHROMEOS)
  // Cros settings and device settings are needed when creating user manager.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  // Needed for creating ExtensionService.
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  TestingProfile profile_;

  EasyUnlockAppEventConsumer event_consumer_;
  ExtensionService* extension_service_;
  TestEventRouter* event_router_;

  base::CommandLine command_line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAppManagerTest);
};

TEST_F(EasyUnlockAppManagerTest, LoadAppWhenNotLoaded) {
  SetExtensionSystemReady();

  // Sanity check for the test: the easy unlock app should not be loaded at
  // this point.
  ASSERT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
  EXPECT_TRUE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, LoadAppWhenAlreadyLoaded) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, LoadAppPreviouslyDisabled) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  extension_service_->DisableExtension(extension_misc::kEasyUnlockAppId,
                                       extensions::Extension::DISABLE_RELOAD);

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  ASSERT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));

  app_manager_->LoadApp();

  ASSERT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, ReloadApp) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());

  ExtensionReloadTracker reload_tracker(&profile_,
                                        extension_misc::kEasyUnlockAppId);
  ASSERT_FALSE(reload_tracker.HasReloaded());

  app_manager_->ReloadApp();

  EXPECT_TRUE(reload_tracker.HasReloaded());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, ReloadAppDisabled) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  extension_service_->DisableExtension(extension_misc::kEasyUnlockAppId,
                                       extensions::Extension::DISABLE_RELOAD);
  ExtensionReloadTracker reload_tracker(&profile_,
                                        extension_misc::kEasyUnlockAppId);
  ASSERT_FALSE(reload_tracker.HasReloaded());

  app_manager_->ReloadApp();

  EXPECT_FALSE(reload_tracker.HasReloaded());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  EXPECT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, DisableApp) {
  SetExtensionSystemReady();

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));

  app_manager_->DisableAppIfLoaded();

  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));
  EXPECT_FALSE(
      extension_service_->IsExtensionEnabled(extension_misc::kEasyUnlockAppId));
}

TEST_F(EasyUnlockAppManagerTest, DisableAppWhenNotLoaded) {
  SetExtensionSystemReady();

  EXPECT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  app_manager_->DisableAppIfLoaded();

  EXPECT_FALSE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, true));

  extension_service_->component_loader()->Add(IDR_EASY_UNLOCK_MANIFEST,
                                              GetAppPath());
  EXPECT_TRUE(extension_service_->GetExtensionById(
      extension_misc::kEasyUnlockAppId, false));
}

TEST_F(EasyUnlockAppManagerTest, EnsureReady) {
  bool ready = false;
  app_manager_->EnsureReady(base::Bind(&VerifyFalseAndSetToTrue, &ready));

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(ready);

  SetExtensionSystemReady();
  ASSERT_TRUE(ready);
}

TEST_F(EasyUnlockAppManagerTest, EnsureReadyAfterExtesionSystemReady) {
  SetExtensionSystemReady();

  bool ready = false;
  app_manager_->EnsureReady(base::Bind(&VerifyFalseAndSetToTrue, &ready));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ready);
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetup) {
  SetExtensionSystemReady();

  ASSERT_EQ(0u, event_consumer_.app_launched_count());

  app_manager_->LoadApp();
  app_manager_->LaunchSetup();

  EXPECT_EQ(1u, event_consumer_.app_launched_count());
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetupWhenDisabled) {
  SetExtensionSystemReady();

  ASSERT_EQ(0u, event_consumer_.app_launched_count());

  app_manager_->LoadApp();
  app_manager_->DisableAppIfLoaded();

  app_manager_->LaunchSetup();

  EXPECT_EQ(0u, event_consumer_.app_launched_count());
}

TEST_F(EasyUnlockAppManagerTest, LaunchSetupWhenNotLoaded) {
  SetExtensionSystemReady();

  ASSERT_EQ(0u, event_consumer_.app_launched_count());

  app_manager_->LaunchSetup();

  EXPECT_EQ(0u, event_consumer_.app_launched_count());
}

TEST_F(EasyUnlockAppManagerTest, SendUserUpdated) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      easy_unlock_private_api::OnUserInfoUpdated::kEventName,
      extension_misc::kEasyUnlockAppId);

  ASSERT_EQ(0u, event_consumer_.user_updated_count());

  EXPECT_TRUE(app_manager_->SendUserUpdatedEvent("user", true /* logged_in */,
                                                 false /* data_ready */));

  EXPECT_EQ(1u, event_consumer_.user_updated_count());

  EXPECT_EQ("user", event_consumer_.user_id());
  EXPECT_TRUE(event_consumer_.user_logged_in());
  EXPECT_FALSE(event_consumer_.user_data_ready());
}

TEST_F(EasyUnlockAppManagerTest, SendUserUpdatedInvertedFlags) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      easy_unlock_private_api::OnUserInfoUpdated::kEventName,
      extension_misc::kEasyUnlockAppId);

  ASSERT_EQ(0u, event_consumer_.user_updated_count());

  EXPECT_TRUE(app_manager_->SendUserUpdatedEvent("user", false /* logged_in */,
                                                 true /* data_ready */));

  EXPECT_EQ(1u, event_consumer_.user_updated_count());

  EXPECT_EQ("user", event_consumer_.user_id());
  EXPECT_FALSE(event_consumer_.user_logged_in());
  EXPECT_TRUE(event_consumer_.user_data_ready());
}

TEST_F(EasyUnlockAppManagerTest, SendUserUpdatedNoRegisteredListeners) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();

  ASSERT_EQ(0u, event_consumer_.user_updated_count());

  EXPECT_FALSE(app_manager_->SendUserUpdatedEvent("user", true, true));
  EXPECT_EQ(0u, event_consumer_.user_updated_count());
}

TEST_F(EasyUnlockAppManagerTest, SendUserUpdatedAppDisabled) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      easy_unlock_private_api::OnUserInfoUpdated::kEventName,
      extension_misc::kEasyUnlockAppId);
  app_manager_->DisableAppIfLoaded();

  ASSERT_EQ(0u, event_consumer_.user_updated_count());

  EXPECT_FALSE(app_manager_->SendUserUpdatedEvent("user", true, true));
  EXPECT_EQ(0u, event_consumer_.user_updated_count());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttempted) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      screenlock_private_api::OnAuthAttempted::kEventName,
      extension_misc::kEasyUnlockAppId);

  ASSERT_EQ(0u, event_consumer_.user_updated_count());

  EXPECT_TRUE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(1u, event_consumer_.auth_attempted_count());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttemptedNoRegisteredListeners) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();

  ASSERT_EQ(0u, event_consumer_.auth_attempted_count());

  EXPECT_FALSE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(0u, event_consumer_.auth_attempted_count());
}

TEST_F(EasyUnlockAppManagerTest, SendAuthAttemptedAppDisabled) {
  SetExtensionSystemReady();

  app_manager_->LoadApp();
  event_router_->AddLazyEventListener(
      screenlock_private_api::OnAuthAttempted::kEventName,
      extension_misc::kEasyUnlockAppId);
  app_manager_->DisableAppIfLoaded();

  ASSERT_EQ(0u, event_consumer_.auth_attempted_count());

  EXPECT_FALSE(app_manager_->SendAuthAttemptEvent());
  EXPECT_EQ(0u, event_consumer_.auth_attempted_count());
}

}  // namespace
