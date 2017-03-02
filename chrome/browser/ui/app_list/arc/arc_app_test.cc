// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_test.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_play_store_enabled_preference_handler.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName1[] = "fake.package.name1";
constexpr char kPackageName2[] = "fake.package.name2";
constexpr char kPackageName3[] = "fake.package.name3";
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::ShortcutInfo& shortcut) {
  return ArcAppListPrefs::GetAppId(shortcut.package_name, shortcut.intent_uri);
}

ArcAppTest::ArcAppTest() {
  user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
      new chromeos::FakeChromeUserManager()));
  CreateFakeAppsAndPackages();
}

ArcAppTest::~ArcAppTest() {
}

chromeos::FakeChromeUserManager* ArcAppTest::GetUserManager() {
  return static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
}

void ArcAppTest::SetUp(Profile* profile) {
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::Initialize();
    dbus_thread_manager_initialized_ = true;
  }
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  DCHECK(!profile_);
  profile_ = profile;
  const user_manager::User* user = CreateUserAndLogin();

  // If for any reason the garbage collector kicks in while we are waiting for
  // an icon, have the user-to-profile mapping ready to avoid using the real
  // profile manager (which is null).
  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                    profile_);

  // A valid |arc_app_list_prefs_| is needed for the ARC bridge service and the
  // ARC auth service.
  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
  if (!arc_app_list_pref_) {
    ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
        profile_);
  }
  arc_service_manager_ = base::MakeUnique<arc::ArcServiceManager>(nullptr);
  arc_session_manager_ = base::MakeUnique<arc::ArcSessionManager>(
      base::MakeUnique<arc::ArcSessionRunner>(
          base::Bind(arc::FakeArcSession::Create)));
  DCHECK(arc::ArcSessionManager::Get());
  arc::ArcSessionManager::DisableUIForTesting();
  arc::ArcAuthNotification::DisableForTesting();
  arc_session_manager_->SetProfile(profile_);
  arc_play_store_enabled_preference_handler_ =
      base::MakeUnique<arc::ArcPlayStoreEnabledPreferenceHandler>(
          profile_, arc_session_manager_.get());
  arc_play_store_enabled_preference_handler_->Start();

  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_app_list_pref_);
  base::RunLoop run_loop;
  arc_app_list_pref_->SetDefaltAppsReadyCallback(run_loop.QuitClosure());
  run_loop.Run();

  arc::SetArcPlayStoreEnabledForProfile(profile_, true);
  // Check initial conditions.
  EXPECT_FALSE(arc_session_manager_->IsSessionRunning());

  app_instance_.reset(new arc::FakeAppInstance(arc_app_list_pref_));
  arc_service_manager_->arc_bridge_service()->app()->SetInstance(
      app_instance_.get());
}

void ArcAppTest::CreateFakeAppsAndPackages() {
  arc::mojom::AppInfo app;
  // Make sure we have enough data for test.
  for (int i = 0; i < 3; ++i) {
    app.name = base::StringPrintf("Fake App %d", i);
    app.package_name = base::StringPrintf("fake.app.%d", i);
    app.activity = base::StringPrintf("fake.app.%d.activity", i);
    app.sticky = false;
    fake_apps_.push_back(app);
  }
  fake_apps_[0].sticky = true;

  app.name = "TestApp1";
  app.package_name = "test.app1";
  app.activity = "test.app1.activity";
  app.sticky = true;
  fake_default_apps_.push_back(app);

  app.name = "TestApp2";
  app.package_name = "test.app2";
  app.activity = "test.app2.activity";
  app.sticky = true;
  fake_default_apps_.push_back(app);

  arc::mojom::ArcPackageInfo package1;
  package1.package_name = kPackageName1;
  package1.package_version = 1;
  package1.last_backup_android_id = 1;
  package1.last_backup_time = 1;
  package1.sync = false;
  fake_packages_.push_back(package1);

  arc::mojom::ArcPackageInfo package2;
  package2.package_name = kPackageName2;
  package2.package_version = 2;
  package2.last_backup_android_id = 2;
  package2.last_backup_time = 2;
  package2.sync = true;
  fake_packages_.push_back(package2);

  arc::mojom::ArcPackageInfo package3;
  package3.package_name = kPackageName3;
  package3.package_version = 3;
  package3.last_backup_android_id = 3;
  package3.last_backup_time = 3;
  package3.sync = false;
  fake_packages_.push_back(package3);

  for (int i = 0; i < 3; ++i) {
    arc::mojom::ShortcutInfo shortcut_info;
    shortcut_info.name = base::StringPrintf("Fake Shortcut %d", i);
    shortcut_info.package_name = base::StringPrintf("fake.shortcut.%d", i);
    shortcut_info.intent_uri =
        base::StringPrintf("#Intent;fake.shortcut.%d.intent_uri", i);
    shortcut_info.icon_resource_id =
        base::StringPrintf("fake.shortcut.%d.icon_resource_id", i);
    fake_shortcuts_.push_back(shortcut_info);
  }
}

void ArcAppTest::TearDown() {
  app_instance_.reset();
  arc_play_store_enabled_preference_handler_.reset();
  arc_session_manager_.reset();
  arc_service_manager_.reset();
  if (dbus_thread_manager_initialized_) {
    // DBusThreadManager may be initialized from other testing utility,
    // such as ash::test::AshTestHelper::SetUp(), so Shutdown() only when
    // it is initialized in ArcAppTest::SetUp().
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }
  profile_ = nullptr;
}

void ArcAppTest::StopArcInstance() {
  arc_service_manager_->arc_bridge_service()->app()->SetInstance(nullptr);
}

void ArcAppTest::RestartArcInstance() {
  auto* bridge_service = arc_service_manager_->arc_bridge_service();
  bridge_service->app()->SetInstance(nullptr);
  app_instance_ = base::MakeUnique<arc::FakeAppInstance>(arc_app_list_pref_);
  bridge_service->app()->SetInstance(app_instance_.get());
}

const user_manager::User* ArcAppTest::CreateUserAndLogin() {
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile_->GetProfileUserName(), "1234567890"));
  const user_manager::User* user = GetUserManager()->AddUser(account_id);
  GetUserManager()->LoginUser(account_id);
  return user;
}

void ArcAppTest::AddPackage(const arc::mojom::ArcPackageInfo& package) {
  if (!FindPackage(package))
    fake_packages_.push_back(package);
}

void ArcAppTest::RemovePackage(const arc::mojom::ArcPackageInfo& package) {
  std::vector<arc::mojom::ArcPackageInfo>::iterator iter;
  for (iter = fake_packages_.begin(); iter != fake_packages_.end(); iter++) {
    if ((*iter).package_name == package.package_name) {
      fake_packages_.erase(iter);
      break;
    }
  }
}

bool ArcAppTest::FindPackage(const arc::mojom::ArcPackageInfo& package) {
  for (auto fake_package : fake_packages_) {
    if (package.package_name == fake_package.package_name)
      return true;
  }
  return false;
}
