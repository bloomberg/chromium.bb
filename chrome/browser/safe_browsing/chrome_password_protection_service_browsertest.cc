// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kGaiaPasswordChangeHistogramName[] =
    "PasswordProtection.GaiaPasswordReusesBeforeGaiaPasswordChanged";
const char kLoginPageUrl[] = "/safe_browsing/login_page.html";

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

  ChromePasswordProtectionService* GetService(bool is_incognito) {
    return ChromePasswordProtectionService::GetPasswordProtectionService(
        is_incognito ? browser()->profile()->GetOffTheRecordProfile()
                     : browser()->profile());
  }

  void SimulateGaiaPasswordChange(bool is_incognito) {
    if (is_incognito) {
      browser()->profile()->GetOffTheRecordProfile()->GetPrefs()->SetString(
          password_manager::prefs::kSyncPasswordHash, "new_password_hash");
    } else {
      browser()->profile()->GetPrefs()->SetString(
          password_manager::prefs::kSyncPasswordHash, "new_password_hash");
    }
  }

  void SimulateAction(ChromePasswordProtectionService* service,
                      ChromePasswordProtectionService::WarningUIType ui_type,
                      ChromePasswordProtectionService::WarningAction action) {
    for (auto& observer : service->observer_list_) {
      if (ui_type == observer.GetObserverType()) {
        observer.InvokeActionForTesting(action);
      }
    }
  }

  void SimulateGaiaPasswordChanged(ChromePasswordProtectionService* service) {
    service->OnGaiaPasswordChanged();
  }

  void GetSecurityInfo(content::WebContents* web_contents,
                       security_state::SecurityInfo* out_security_info) {
    SecurityStateTabHelper* helper =
        SecurityStateTabHelper::FromWebContents(web_contents);
    helper->GetSecurityInfo(out_security_info);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;
};

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       SuccessfullyChangePassword) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking "Change Password" button on the modal dialog.
  // There should be only 1 observer in the list.
  SimulateAction(service, ChromePasswordProtectionService::MODAL_DIALOG,
                 ChromePasswordProtectionService::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(
      new_web_contents, /*number_of_navigations=*/1,
      content::MessageLoopRunner::QuitMode::DEFERRED);
  observer.Wait();
  // chrome://settings page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(GURL(chrome::kChromeUISettingsURL),
            new_web_contents->GetVisibleURL());

  // Simulates clicking "Change password" button on the chrome://settings card.
  SimulateAction(service, ChromePasswordProtectionService::CHROME_SETTINGS,
                 ChromePasswordProtectionService::CHANGE_PASSWORD);
  base::RunLoop().RunUntilIdle();
  // Verify myaccount.google.com or Google signin page should be opened in a
  // new foreground tab.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetVisibleURL()
                  .DomainIs("google.com"));

  // Simulates user finished changing password.
  SimulateGaiaPasswordChanged(service);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  // TODO(jialiul): Check malicious content status here after crbug.com/762738
  // closes.
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       MarkSiteAsLegitimate) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;

  // Initialize and verify initial state.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::NONE, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking "Ignore" button on the modal dialog.
  // There should be only 1 observer in the list.
  SimulateAction(service, ChromePasswordProtectionService::MODAL_DIALOG,
                 ChromePasswordProtectionService::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  // No new tab opens. SecurityInfo doesn't change.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking on "Mark site legitimate". Site is no longer dangerous.
  service->OnUserAction(web_contents,
                        ChromePasswordProtectionService::PAGE_INFO,
                        ChromePasswordProtectionService::MARK_AS_LEGITIMATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  EXPECT_EQ(security_state::NONE, security_info.security_level);
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       OpenChromeSettingsViaPageInfo) {
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  Profile* profile = browser()->profile();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  security_state::SecurityInfo security_info;
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));

  // Shows modal dialog on current web_contents.
  service->ShowModalWarning(web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  // Simulates clicking "Ignore" to close dialog.
  SimulateAction(service, ChromePasswordProtectionService::MODAL_DIALOG,
                 ChromePasswordProtectionService::IGNORE_WARNING);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  GetSecurityInfo(web_contents, &security_info);
  ASSERT_EQ(security_state::DANGEROUS, security_info.security_level);
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_PASSWORD_REUSE,
            security_info.malicious_content_status);

  // Simulates clicking on "Change Password" in the page info bubble.
  service->OnUserAction(web_contents,
                        ChromePasswordProtectionService::PAGE_INFO,
                        ChromePasswordProtectionService::CHANGE_PASSWORD);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(
      new_web_contents, /*number_of_navigations=*/1,
      content::MessageLoopRunner::QuitMode::DEFERRED);
  observer.Wait();
  // chrome://settings page should be opened in a new foreground tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(
      GURL(chrome::kChromeUISettingsURL),
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyUnhandledPasswordReuse) {
  histograms_.ExpectTotalCount(kGaiaPasswordChangeHistogramName, 0);
  ChromePasswordProtectionService* service = GetService(/*is_incognito=*/false);
  ASSERT_TRUE(service);
  Profile* profile = browser()->profile();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kLoginPageUrl));
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
  Browser* browser2 = CreateBrowser(profile);
  // Shows modal dialog on this new web_contents.
  content::WebContents* new_web_contents =
      browser2->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser2, GURL("data:text/html,<html></html>"));
  service->ShowModalWarning(new_web_contents, "unused_token");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, service->unhandled_password_reuses().size());
  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChange(/*is_incognito=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service->unhandled_password_reuses().empty());
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_THAT(histograms_.GetAllSamples(kGaiaPasswordChangeHistogramName),
              testing::ElementsAre(base::Bucket(2, 1)));
}

IN_PROC_BROWSER_TEST_F(ChromePasswordProtectionServiceBrowserTest,
                       VerifyShouldShowChangePasswordSettingUI) {
  Profile* profile = browser()->profile();
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  // Simulates previous session has unhandled password reuses.
  profile->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingChangePasswordInSettingsEnabled, true);

  EXPECT_TRUE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));

  // Simulates a Gaia password change.
  SimulateGaiaPasswordChanged(GetService(/*is_incognito=*/false));
  EXPECT_FALSE(
      ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
          profile));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingChangePasswordInSettingsEnabled));
}

// TODO(jialiul): Add more tests where multiple browser windows are involved.

}  // namespace safe_browsing
