// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_test.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPackageName1[] = "fakepackagename1";
constexpr char kPackageName2[] = "fakepackagename2";
constexpr char kPackageName3[] = "fakepackagename3";
}

// static
std::string ArcAppTest::GetAppId(const arc::mojom::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
}

std::string ArcAppTest::GetAppId(const arc::mojom::ShortcutInfo& shortcut) {
  return ArcAppListPrefs::GetAppId(shortcut.package_name, shortcut.intent_uri);
}

ArcAppTest::ArcAppTest() {
  user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
      new chromeos::FakeChromeUserManager()));
}

ArcAppTest::~ArcAppTest() {
}

chromeos::FakeChromeUserManager* ArcAppTest::GetUserManager() {
  return static_cast<chromeos::FakeChromeUserManager*>(
      user_manager::UserManager::Get());
}

void ArcAppTest::SetUp(Profile* profile) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableArc);
  DCHECK(!profile_);
  profile_ = profile;
  CreateUserAndLogin();

  // Make sure we have enough data for test.
  for (int i = 0; i < 3; ++i) {
    arc::mojom::AppInfo app;
    app.name = base::StringPrintf("Fake App %d", i);
    app.package_name = base::StringPrintf("fake.app.%d", i);
    app.activity = base::StringPrintf("fake.app.%d.activity", i);
    app.sticky = false;
    fake_apps_.push_back(app);
  }
  fake_apps_[0].sticky = true;

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
    arc::mojom::ShortcutInfo shortcutInfo;
    shortcutInfo.name = base::StringPrintf("Fake Shortcut %d", i);
    shortcutInfo.package_name = base::StringPrintf("fake.shortcut.%d", i);
    shortcutInfo.intent_uri =
        base::StringPrintf("fake.shortcut.%d.intent_uri", i);
    shortcutInfo.icon_resource_id =
        base::StringPrintf("fake.shortcut.%d.icon_resource_id", i);
    fake_shortcuts_.push_back(shortcutInfo);
  }

  bridge_service_.reset(new arc::FakeArcBridgeService());

  auth_service_.reset(new arc::ArcAuthService(bridge_service_.get()));
  DCHECK(arc::ArcAuthService::Get());
  arc::ArcAuthService::DisableUIForTesting();
  arc_auth_service()->OnPrimaryUserProfilePrepared(profile_);
  auth_service_->EnableArc();

  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_app_list_pref_);

  app_instance_.reset(new arc::FakeAppInstance(arc_app_list_pref_));
  arc::mojom::AppInstancePtr instance;
  app_instance_->Bind(mojo::GetProxy(&instance));
  bridge_service_->OnAppInstanceReady(std::move(instance));
  app_instance_->WaitForOnAppInstanceReady();

  // Check initial conditions.
  EXPECT_EQ(bridge_service_.get(), arc::ArcBridgeService::Get());
  EXPECT_TRUE(!arc::ArcBridgeService::Get()->available());
  EXPECT_EQ(arc::ArcBridgeService::State::STOPPED,
            arc::ArcBridgeService::Get()->state());

  // At this point we should have ArcAppListPrefs as observer of service.
  EXPECT_TRUE(bridge_service_->HasObserver(arc_app_list_pref_));
  bridge_service()->SetReady();
}

void ArcAppTest::TearDown() {
  auth_service_.reset();
}

void ArcAppTest::CreateUserAndLogin() {
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      profile_->GetProfileUserName(), "1234567890"));
  GetUserManager()->AddUser(account_id);
  GetUserManager()->LoginUser(account_id);
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
