// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"

#include "chrome/browser/policy/profile_policy_connector.h"
#include "components/policy/core/common/policy_service.h"

// Handles requests for prefs::kCookieControlsMode retrival/update.
class CookieControlsServiceObserver : public CookieControlsService::Observer {
 public:
  explicit CookieControlsServiceObserver(Profile* profile) {
    service_ = CookieControlsServiceFactory::GetForProfile(profile);
    service_->AddObserver(this);
    checked_ = false;
  }

  ~CookieControlsServiceObserver() override = default;

  CookieControlsService* GetService() { return service_; }
  void SetChecked(bool checked) { checked_ = checked; }
  bool GetChecked() { return checked_; }

  // CookieControlsService::Observer
  void OnThirdPartyCookieBlockingPrefChanged() override {
    SetChecked(service_->GetToggleCheckedValue());
  }

 private:
  CookieControlsService* service_;
  bool checked_;

  DISALLOW_COPY_AND_ASSIGN(CookieControlsServiceObserver);
};

class CookieControlsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitAndEnableFeature(
        content_settings::kImprovedCookieControls);
    web_ui_.set_web_contents(web_contents());
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<CookieControlsServiceObserver> observer_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CookieControlsServiceTest, HandleCookieControlsToggleChanged) {
  Profile* profile = Profile::FromBrowserContext(
      web_ui_.GetWebContents()->GetBrowserContext());
  observer_ = std::make_unique<CookieControlsServiceObserver>(profile);
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  // Set toggle value to false
  observer_->SetChecked(true);
  observer_->GetService()->HandleCookieControlsToggleChanged(false);
  EXPECT_EQ(static_cast<int>(content_settings::CookieControlsMode::kOff),
            profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));
  EXPECT_EQ(observer_->GetChecked(), false);

  // Set toggle value to true
  observer_->GetService()->HandleCookieControlsToggleChanged(true);
  EXPECT_EQ(
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly),
      profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  // TestingProfile does not have a PolicyService for incognito and this
  // should not create a checked value of "true" in normal mode.
  EXPECT_EQ(observer_->GetChecked(), false);
}
