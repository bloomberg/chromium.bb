// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_test.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
std::string ArcAppTest::GetAppId(const arc::mojom::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
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
  DCHECK(!profile_);
  profile_ = profile;

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

  bridge_service_.reset(new arc::FakeArcBridgeService());

  auth_service_.reset(new arc::ArcAuthService(bridge_service_.get()));
  DCHECK(arc::ArcAuthService::Get());

  arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);

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
}

void ArcAppTest::CreateUserAndLogin() {
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(),
                                     "1234567890"));
  GetUserManager()->AddUser(account_id);
  GetUserManager()->LoginUser(account_id);

  arc::ArcAuthService::DisableUIForTesting();
  arc_auth_service()->OnPrimaryUserProfilePrepared(profile_);
  arc_auth_service()->EnableArc();
  bridge_service()->SetReady();
}

void ArcAppTest::TearDown() {
  auth_service_.reset();
}
