// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kGaiaPasswordChangeHistogramName[] =
    "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged";

}  // namespace

namespace safe_browsing {

class ChromePasswordProtectionServiceBrowserTest : public InProcessBrowserTest {
 public:
  ChromePasswordProtectionServiceBrowserTest() {}

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    scoped_feature_list_.InitAndEnableFeature(kGoogleBrandedPhishingWarning);
    InProcessBrowserTest::SetUp();
  }

  ChromePasswordProtectionService* GetService() {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        browser()->profile());
  }

  void SimulateGaiaPasswordChange() {
    browser()->profile()->GetPrefs()->SetString(
        password_manager::prefs::kSyncPasswordHash, "new_password_hash");
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VeriryUnhandledPasswordReuse) {
  histograms_.ExpectTotalCount(kGaiaPasswordChangeHistogramName, 0);
  ChromePasswordProtectionService* service = GetService();
  ASSERT_TRUE(service);
  Profile* profile = browser()->profile();
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));
  ASSERT_TRUE(service->unhandled_password_reuses().empty());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Shows modal dialog on current web_contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  service->ShowModalWarning(web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, service->unhandled_password_reuses().size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Opens a new browser window.
  Browser* browser2 = CreateBrowser(browser()->profile());
  // Shows modal dialog on this new web_contents.
  content::WebContents* new_web_contents =
      browser2->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL("/"));
  service->ShowModalWarning(new_web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, service->unhandled_password_reuses().size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChange();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, service->unhandled_password_reuses().size());
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_THAT(histograms_.GetAllSamples(kGaiaPasswordChangeHistogramName),
              testing::ElementsAre(base::Bucket(2, 1)));
}

}  // namespace safe_browsing
