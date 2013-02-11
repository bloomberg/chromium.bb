// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_to_mobile_service.h"

#include "base/prefs/pref_service.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_to_mobile_service_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

using extensions::FeatureSwitch;

class ChromeToMobileServiceTest : public BrowserWithTestWindowTest {
 public:
  ChromeToMobileServiceTest();
  virtual ~ChromeToMobileServiceTest();

  // Get the ChromeToMobileService for the browser's profile.
  ChromeToMobileService* GetService() const;

  // Ensures the returned command state matches UpdateAndGetCommandState().
  bool UpdateAndGetVerifiedCommandState();

  // Set |count| available mock mobile devices in the profile' prefs.
  void SetDeviceCount(size_t count);

  // Fulfill the requirements to enabling the feature.
  void FulfillFeatureRequirements();

 private:
  FeatureSwitch::ScopedOverride enable_action_box_;

  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileServiceTest);
};

// Chrome To Mobile is currently gated on the Action Box UI,
// so need to enable this feature for the test.
ChromeToMobileServiceTest::ChromeToMobileServiceTest()
    : enable_action_box_(FeatureSwitch::action_box(), true) {}

ChromeToMobileServiceTest::~ChromeToMobileServiceTest() {}

ChromeToMobileService* ChromeToMobileServiceTest::GetService() const {
  return ChromeToMobileServiceFactory::GetForProfile(profile());
}

bool ChromeToMobileServiceTest::UpdateAndGetVerifiedCommandState() {
  bool state = ChromeToMobileService::UpdateAndGetCommandState(browser());
  CommandUpdater* updater = browser()->command_controller()->command_updater();
  EXPECT_EQ(state, updater->IsCommandEnabled(IDC_CHROME_TO_MOBILE_PAGE));
  return state;
}

void ChromeToMobileServiceTest::SetDeviceCount(size_t count) {
  ListValue mobiles;
  for (size_t i = 0; i < count; ++i)
    mobiles.Append(new DictionaryValue());
  profile()->GetPrefs()->Set(prefs::kChromeToMobileDeviceList, mobiles);
}

void ChromeToMobileServiceTest::FulfillFeatureRequirements() {
  AddTab(browser(), GURL("http://foo"));
  SetDeviceCount(1);
  GetService()->OnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(UpdateAndGetVerifiedCommandState());
}

// Test that GetMobiles and HasMobiles require Sync Invalidations being enabled.
TEST_F(ChromeToMobileServiceTest, GetMobiles) {
  ChromeToMobileService* service = GetService();
  EXPECT_EQ(NULL, service->GetMobiles());
  EXPECT_FALSE(service->HasMobiles());

  // Add a mock mobile device to the profile prefs.
  SetDeviceCount(1);

  // GetMobiles() still returns NULL until Sync Invalidations are enabled.
  EXPECT_EQ(NULL, service->GetMobiles());
  EXPECT_FALSE(service->HasMobiles());
  service->OnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_EQ(1U, service->GetMobiles()->GetSize());
  EXPECT_TRUE(service->HasMobiles());

  // Adding and removing devices works while Sync Invalidations are enabled.
  SetDeviceCount(0);
  EXPECT_EQ(0U, service->GetMobiles()->GetSize());
  EXPECT_FALSE(service->HasMobiles());
  SetDeviceCount(2);
  EXPECT_EQ(2U, service->GetMobiles()->GetSize());
  EXPECT_TRUE(service->HasMobiles());

  // GetMobiles() returns NULL after Sync Invalidations are disabled.
  service->OnInvalidatorStateChange(syncer::TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(NULL, service->GetMobiles());
  EXPECT_FALSE(service->HasMobiles());
  service->OnInvalidatorStateChange(syncer::INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(NULL, service->GetMobiles());
  EXPECT_FALSE(service->HasMobiles());
}

// Test fulfilling the requirements to enable the feature.
TEST_F(ChromeToMobileServiceTest, RequirementsToEnable) {
  // Navigate to a page with a URL that is valid to send.
  AddTab(browser(), GURL("http://foo"));
  EXPECT_FALSE(UpdateAndGetVerifiedCommandState());

  // Add a mock mobile device to the profile prefs.
  SetDeviceCount(1);
  EXPECT_FALSE(UpdateAndGetVerifiedCommandState());

  // Send a fake notification that Sync Invalidations are enabled.
  GetService()->OnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(UpdateAndGetVerifiedCommandState());
}

// Test that the feature handles mobile device count changes properly.
TEST_F(ChromeToMobileServiceTest, MobileDevicesAreRequired) {
  FulfillFeatureRequirements();

  // Removing all devices disables the feature.
  SetDeviceCount(0);
  EXPECT_FALSE(UpdateAndGetVerifiedCommandState());

  // Restoring mobile devices re-enables the feature.
  SetDeviceCount(1);
  EXPECT_TRUE(UpdateAndGetVerifiedCommandState());
  SetDeviceCount(3);
  EXPECT_TRUE(UpdateAndGetVerifiedCommandState());
}

// Test that the feature handles Sync Invalidations state changes properly.
TEST_F(ChromeToMobileServiceTest, SyncInvalidationsAreRequired) {
  FulfillFeatureRequirements();

  // Disabling Sync Invalidations disables the feature.
  GetService()->OnInvalidatorStateChange(syncer::TRANSIENT_INVALIDATION_ERROR);
  EXPECT_FALSE(UpdateAndGetVerifiedCommandState());

  // Re-enabling Sync Invalidations re-enables the feature.
  GetService()->OnInvalidatorStateChange(syncer::INVALIDATIONS_ENABLED);
  EXPECT_TRUE(UpdateAndGetVerifiedCommandState());
}

// Test that the feature handles various page URLs properly.
TEST_F(ChromeToMobileServiceTest, CertainSchemesAreRequired) {
  FulfillFeatureRequirements();

  // Only http:, https:, and ftp: URLs are valid to send.
  struct {
    std::string url;
    bool enabled;
  } cases[] = {
    { "http://foo", true }, { "https://foo", true }, { "ftp://foo", true },
    { "about:foo", false }, { "chrome://foo", false }, { "file://foo", false },
    { "data://foo", false }, { "view-source:foo", false },
  };

  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    NavigateAndCommit(controller, GURL(cases[i].url));
    EXPECT_EQ(cases[i].enabled, UpdateAndGetVerifiedCommandState());
  }
}

// Ensure that only relevant notifications invalidate the access token.
TEST_F(ChromeToMobileServiceTest, TokenNotifications) {
  const char dummy_string[] = "dummy";
  ChromeToMobileService* service = GetService();
  service->SetAccessTokenForTest(dummy_string);
  ASSERT_FALSE(service->GetAccessTokenForTest().empty());

  // Send dummy service/token details (should not clear the token).
  TokenService::TokenAvailableDetails dummy_details(dummy_string, dummy_string);
  service->Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<ChromeToMobileServiceTest>(this),
      content::Details<TokenService::TokenAvailableDetails>(&dummy_details));
  EXPECT_FALSE(service->GetAccessTokenForTest().empty());

  // Send a Gaia OAuth2 Login service dummy token (should clear the token).
  TokenService::TokenAvailableDetails login_details(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, dummy_string);
  service->Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<ChromeToMobileServiceTest>(this),
      content::Details<TokenService::TokenAvailableDetails>(&login_details));
  EXPECT_TRUE(service->GetAccessTokenForTest().empty());
}

}  // namespace
