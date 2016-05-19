// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

namespace arc {
namespace mojom {
class AppInfo;
}
class ArcAuthService;
class FakeArcBridgeService;
class FakeAppInstance;
}

namespace chromeos {
class FakeChromeUserManager;
class ScopedUserManagerEnabler;
}

class ArcAppListPrefs;
class Profile;

// Helper class to initialize arc bridge to work with arc apps in unit tests.
class ArcAppTest {
 public:
  ArcAppTest();
  virtual ~ArcAppTest();

  void SetUp(Profile* profile);
  void CreateUserAndLogin();
  void TearDown();

  static std::string GetAppId(const arc::mojom::AppInfo& app_info);

  // The 0th item is sticky but not the followings.
  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return fake_apps_;
  }

  chromeos::FakeChromeUserManager* GetUserManager();

  arc::FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  ArcAppListPrefs* arc_app_list_prefs() { return arc_app_list_pref_; }

  arc::ArcAuthService* arc_auth_service() { return auth_service_.get(); }

 private:
  // Unowned pointer.
  Profile* profile_ = nullptr;

  ArcAppListPrefs* arc_app_list_pref_ = nullptr;

  std::unique_ptr<arc::FakeArcBridgeService> bridge_service_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<arc::ArcAuthService> auth_service_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  std::vector<arc::mojom::AppInfo> fake_apps_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppTest);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_TEST_H_
