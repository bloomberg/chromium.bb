// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#include <map>

#include "base/bind.h"
#import "base/mac/bind_objc_block.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using chrome_test_util::buttonWithAccessibilityLabelId;

namespace {

const char kTestOrigin1[] = "http://host1:1/";

const char kUrl[] = "http://foo/browsing";
const char kUrlWithSetCookie[] = "http://foo/set_cookie";
const char kResponse[] = "bar";
const char kResponseWithSetCookie[] = "bar with set cookie";
const char kNoCookieText[] = "No cookies";
const char kCookie[] = "a=b";
const char kJavaScriptGetCookies[] =
    "var c = document.cookie ? document.cookie.split(/;\\s*/) : [];"
    "if (!c.length) {"
    "  document.documentElement.innerHTML = 'No cookies!';"
    "} else {"
    "  document.documentElement.innerHTML = 'OK: ' + c.join(',');"
    "}";

enum MetricsServiceType {
  kMetrics,
  kBreakpad,
  kBreakpadFirstLaunch,
};

// Matcher for the clear browsing history cell on the clear browsing data panel.
id<GREYMatcher> clearBrowsingHistoryButton() {
  return grey_allOf(grey_accessibilityID(kClearBrowsingHistoryCellId),
                    grey_sufficientlyVisible(), nil);
}
// Matcher for the clear cookies cell on the clear browsing data panel.
id<GREYMatcher> clearCookiesButton() {
  return grey_accessibilityID(kClearCookiesCellId);
}
// Matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> clearCacheButton() {
  return grey_allOf(grey_accessibilityID(kClearCacheCellId),
                    grey_sufficientlyVisible(), nil);
}
// Matcher for the clear saved passwords cell on the clear browsing data panel.
id<GREYMatcher> clearSavedPasswordsButton() {
  return grey_allOf(grey_accessibilityID(kClearSavedPasswordsCellId),
                    grey_sufficientlyVisible(), nil);
}
// Matcher for the clear browsing data button on the clear browsing data panel.
id<GREYMatcher> clearBrowsingDataButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_CLEAR_BUTTON);
}
// Matcher for the done button in the navigation bar.
id<GREYMatcher> navigationDoneButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}
// Matcher for the Settings button in the tools menu.
id<GREYMatcher> settingsButton() {
  return grey_accessibilityID(kToolsMenuSettingsId);
}
// Matcher for the Save Passwords cell on the main Settings screen.
id<GREYMatcher> passwordsButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_SAVE_PASSWORDS);
}
// Matcher for the Privacy cell on the main Settings screen.
id<GREYMatcher> privacyButton() {
  return buttonWithAccessibilityLabelId(
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
}
// Matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> clearBrowsingDataCell() {
  return buttonWithAccessibilityLabelId(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
}

// Matcher for the Search Engine cell on the main Settings screen.
id<GREYMatcher> searchEngineButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE);
}

// Matcher for the Autofill Forms cell on the main Settings screen.
id<GREYMatcher> autofillButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_AUTOFILL);
}

// Matcher for the Google Apps cell on the main Settings screen.
id<GREYMatcher> googleAppsButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_GOOGLE_APPS_SM_SETTINGS);
}

// Matcher for the Google Chrome cell on the main Settings screen.
id<GREYMatcher> googleChromeButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_PRODUCT_NAME);
}

// Matcher for the Google Chrome cell on the main Settings screen.
id<GREYMatcher> voiceSearchButton() {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

// Matcher for the Preload Webpages button on the bandwidth UI.
id<GREYMatcher> BandwidthPreloadWebpagesButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
}

// Matcher for the Privacy Handoff button on the privacy UI.
id<GREYMatcher> PrivacyHandoffButton() {
  return buttonWithAccessibilityLabelId(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
}

// Matcher for the Privacy Block Popups button on the privacy UI.
id<GREYMatcher> BlockPopupsButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_BLOCK_POPUPS);
}

// Matcher for the Privacy Translate Settings button on the privacy UI.
id<GREYMatcher> TranslateSettingsButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_TRANSLATE_SETTING);
}

// Asserts that there is no cookie in current web state.
void AssertNoCookieExists() {
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(
      base::SysUTF8ToNSString(kJavaScriptGetCookies), &error);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                          kNoCookieText)]
      assertWithMatcher:grey_notNil()];
}

// Asserts |cookie| exists in current web state.
void AssertCookieExists(const char cookie[]) {
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(
      base::SysUTF8ToNSString(kJavaScriptGetCookies), &error);
  NSString* const expectedCookieText =
      [NSString stringWithFormat:@"OK: %@", base::SysUTF8ToNSString(cookie)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                   base::SysNSStringToUTF8(expectedCookieText))]
      assertWithMatcher:grey_notNil()];
}

// Run as a task to check if a certificate has been added to the ChannelIDStore.
// Signals the given |semaphore| if the cert was added, or reposts itself
// otherwise.
void CheckCertificate(scoped_refptr<net::URLRequestContextGetter> getter,
                      dispatch_semaphore_t semaphore) {
  net::ChannelIDService* channel_id_service =
      getter->GetURLRequestContext()->channel_id_service();
  if (channel_id_service->channel_id_count() == 0) {
    // If the channel_id_count is still 0, no certs have been added yet.
    // Re-post this task and check again later.
    web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                             base::Bind(&CheckCertificate, getter, semaphore));
  } else {
    // If certs have been added, signal the calling thread.
    dispatch_semaphore_signal(semaphore);
  }
}

// Set certificate for host |kTestOrigin1| for testing.
void SetCertificate() {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  scoped_refptr<net::URLRequestContextGetter> getter =
      browserState->GetRequestContext();
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE, base::BindBlock(^{
        net::ChannelIDService* channel_id_service =
            getter->GetURLRequestContext()->channel_id_service();
        net::ChannelIDStore* channel_id_store =
            channel_id_service->GetChannelIDStore();
        base::Time now = base::Time::Now();
        channel_id_store->SetChannelID(
            base::MakeUnique<net::ChannelIDStore::ChannelID>(
                kTestOrigin1, now, crypto::ECPrivateKey::Create()));
      }));

  // The ChannelIDStore may not be loaded, so adding the new cert may not happen
  // immediately.  This posted task signals the semaphore if the cert was added,
  // or re-posts itself to check again later otherwise.
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                           base::Bind(&CheckCertificate, getter, semaphore));

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  dispatch_release(semaphore);
}

// Fetching channel id is expected to complete immediately in this test, so a
// dummy callback function is set for testing.
void CertCallback(int err,
                  const std::string& server_identifier,
                  std::unique_ptr<crypto::ECPrivateKey> key) {}

// Check if certificate is empty for host |kTestOrigin1|.
bool IsCertificateCleared() {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  __block int result;
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  scoped_refptr<net::URLRequestContextGetter> getter =
      browserState->GetRequestContext();
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE, base::BindBlock(^{
        net::ChannelIDService* channel_id_service =
            getter->GetURLRequestContext()->channel_id_service();
        std::unique_ptr<crypto::ECPrivateKey> dummy_key;
        result = channel_id_service->GetChannelIDStore()->GetChannelID(
            kTestOrigin1, &dummy_key, base::Bind(CertCallback));
        dispatch_semaphore_signal(semaphore);
      }));
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  dispatch_release(semaphore);
  return result == net::ERR_FILE_NOT_FOUND;
}

}  // namespace

// Settings tests for Chrome.
@interface SettingsTestCase : ChromeTestCase
@end

@implementation SettingsTestCase

// Opens the a submenu from the settings page.  Must be called from the NTP.
// TODO(crbug.com/684619): Investigate why usingSearchAction doesn't scroll
// until the bottom.
- (void)openSubSettingMenu:(id<GREYMatcher>)settingToTap {
  const CGFloat kScrollDisplacement = 150.0;
  id<GREYMatcher> toolsMenuTableViewMatcher =
      grey_accessibilityID(kToolsMenuTableViewId);
  id<GREYMatcher> settingsButtonMatcher =
      grey_accessibilityID(kToolsMenuSettingsId);
  id<GREYMatcher> settingsCollectionViewMatcher =
      grey_accessibilityID(kSettingsCollectionViewId);

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:settingsButtonMatcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:toolsMenuTableViewMatcher] performAction:grey_tap()];
  [[[EarlGrey selectElementWithMatcher:settingToTap]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollDisplacement)
      onElementWithMatcher:settingsCollectionViewMatcher]
      performAction:grey_tap()];
}

// Closes a sub-settings menu, and then the general Settings menu.
- (void)closeSubSettingsMenu {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::buttonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

// Performs the steps to clear browsing data. Must be called on the
// Clear Browsing Data settings screen, after having selected the data types
// scheduled for removal.
- (void)clearBrowsingData {
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataButton()]
      performAction:grey_tap()];

  // There is not currently a matcher for accessibilityElementIsFocused or
  // userInteractionEnabled which could be used here instead of checking that
  // the button is not a MDCCollectionViewTextCell. Use when available.
  // TODO(crbug.com/638674): Evaluate if this can move to shared code.
  id<GREYMatcher> confirmClear = grey_allOf(
      clearBrowsingDataButton(),
      grey_not(grey_kindOfClass([MDCCollectionViewTextCell class])), nil);
  [[EarlGrey selectElementWithMatcher:confirmClear] performAction:grey_tap()];
}

// Exits Settings by clicking on the Done button.
- (void)dismissSettings {
  // Dismiss the settings.
  [[EarlGrey selectElementWithMatcher:navigationDoneButton()]
      performAction:grey_tap()];

  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// From the NTP, clears the cookies and site data via the UI.
- (void)clearCookiesAndSiteData {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:privacyButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataCell()]
      performAction:grey_tap()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Uncheck
  // "Browsing history" and "Cached Images and Files".
  // Prefs are reset to default at the end of each test.
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];
  [self dismissSettings];
}

// From the NTP, clears the saved passwords via the UI.
- (void)clearPasswords {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:privacyButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataCell()]
      performAction:grey_tap()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Unckeck all
  // of them and check "Passwords".
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];

  // Re-tap all the previously tapped cells, so that the default state of the
  // checkmarks is preserved.
  [[EarlGrey selectElementWithMatcher:clearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:clearSavedPasswordsButton()]
      performAction:grey_tap()];

  [self dismissSettings];
}

// Checks the presence (or absence) of saved passwords.
// If |saved| is YES, it checks that there is a Saved Passwords section.
// If |saved| is NO, it checks that there is no Saved Passwords section.
- (void)checkIfPasswordsSaved:(BOOL)saved {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:passwordsButton()]
      performAction:grey_tap()];

  id<GREYMatcher> visibilityMatcher =
      saved ? grey_sufficientlyVisible() : grey_notVisible();
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_PASSWORD_MANAGER_SHOW_PASSWORDS_TAB_TITLE))]
      assertWithMatcher:visibilityMatcher];

  // Close the Settings.
  [self closeSubSettingsMenu];
}

// Checks for a given service that it is both recording and uploading, where
// appropriate.
- (void)assertMetricsServiceEnabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics:
      GREYAssertTrue(chrome_test_util::IsMetricsRecordingEnabled(),
                     @"Metrics recording should be enabled.");
      GREYAssertTrue(chrome_test_util::IsMetricsReportingEnabled(),
                     @"Metrics reporting should be enabled.");
      break;
    case kBreakpad:
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertTrue(chrome_test_util::IsBreakpadReportingEnabled(),
                     @"Breakpad reporting should be enabled.");
      break;
    case kBreakpadFirstLaunch:
      // For first launch after upgrade, or after install, uploading of crash
      // reports is always disabled.  Check that the first launch flag is being
      // honored.
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertFalse(chrome_test_util::IsBreakpadReportingEnabled(),
                      @"Breakpad reporting should be disabled.");
      break;
  }
}

// Checks for a given service that it is completely disabled.
- (void)assertMetricsServiceDisabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertFalse(chrome_test_util::IsMetricsRecordingEnabled(),
                      @"Metrics recording should be disabled.");
      GREYAssertFalse(chrome_test_util::IsMetricsReportingEnabled(),
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      // Check only whether or not breakpad is enabled.  Disabling
      // breakpad does stop uploading, and does not change the flag
      // used to check whether or not it's uploading.
      GREYAssertFalse(chrome_test_util::IsBreakpadEnabled(),
                      @"Breakpad should be disabled.");
      break;
    }
  }
}

// Checks for a given service that it is recording, but not uploading anything.
// Used to test that the wifi-only preference is honored when the device is
// using a cellular network.
- (void)assertMetricsServiceEnabledButNotUploading:
    (MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertTrue(chrome_test_util::IsMetricsRecordingEnabled(),
                     @"Metrics recording should be enabled.");
      GREYAssertFalse(chrome_test_util::IsMetricsReportingEnabled(),
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertFalse(chrome_test_util::IsBreakpadReportingEnabled(),
                      @"Breakpad reporting should be disabled.");
      break;
    }
  }
}

- (void)assertsMetricsPrefsForService:(MetricsServiceType)serviceType {
  // Two preferences, each with two values - on or off.  Check all four
  // combinations:
  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
  //  - Services do not record data and do not upload data.

  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
  //  - Services do not record data and do not upload data.
  //    Note that if kMetricsReportingEnabled is OFF, the state of
  //    kMetricsReportingWifiOnly does not matter.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
  //  - Services record data and upload data only when the device is using
  //    a wifi connection.  Note:  rather than checking for wifi, the code
  //    checks for a cellular network (wwan).  wwan != wifi.  So if wwan is
  //    true, services do not upload any data.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
  //  - Services record data and upload data.

  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, NO);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             NO);
  // Service should be completely disabled.
  // I.e. no recording of data, and no uploading of what's been recorded.
  [self assertMetricsServiceDisabled:serviceType];

  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, NO);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             YES);
  // If kMetricsReportingEnabled is OFF, any service should remain completely
  // disabled, i.e. no uploading even if kMetricsReportingWifiOnly is ON.
  [self assertMetricsServiceDisabled:serviceType];

// Split here:  Official build vs. Development build.
// Official builds allow recording and uploading of data, honoring the
// metrics prefs.  Development builds should never record or upload data.
#if defined(GOOGLE_CHROME_BUILD)
  // Official build.
  // The values of the prefs and the wwan vs wifi state should be honored by
  // the services, turning on and off according to the rules laid out above.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON.
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, YES);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             YES);
  // Service should be enabled.
  [self assertMetricsServiceEnabled:serviceType];

  // Set the network to use a cellular network, which should disable uploading
  // when the wifi-only flag is set.
  chrome_test_util::SetWWANStateTo(YES);
  [self assertMetricsServiceEnabledButNotUploading:serviceType];

  // Turn off cellular network usage, which should enable uploading.
  chrome_test_util::SetWWANStateTo(NO);
  [self assertMetricsServiceEnabled:serviceType];

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, YES);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             NO);
  // Service should be always enabled regardless of network settings.
  chrome_test_util::SetWWANStateTo(YES);
  [self assertMetricsServiceEnabled:serviceType];
  chrome_test_util::SetWWANStateTo(NO);
  [self assertMetricsServiceDisabled:serviceType];
#else
  // Development build.  Do not allow any recording or uploading of data.
  // Specifically, the kMetricsReportingEnabled preference is completely
  // disregarded for non-official builds, and checking its value always returns
  // false (NO).
  // This tests that no matter the state change, pref or network connection,
  // services remain disabled.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, YES);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             YES);
  // Service should remain disabled.
  [self assertMetricsServiceDisabled:serviceType];

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, YES);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             NO);
  // Service should remain disabled.
  [self assertMetricsServiceDisabled:serviceType];
#endif
}

#pragma mark Tests

// Tests that clearing the cookies through the UI does clear all of them. Use a
// local server to navigate to a page that sets then tests a cookie, and then
// clears the cookie and tests it is not set.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
- (void)testClearCookies {
  // Creates a map of canned responses and set up the test HTML server.
  std::map<GURL, std::pair<std::string, std::string>> response;

  response[web::test::HttpServer::MakeUrl(kUrlWithSetCookie)] =
      std::pair<std::string, std::string>(kCookie, kResponseWithSetCookie);
  response[web::test::HttpServer::MakeUrl(kUrl)] =
      std::pair<std::string, std::string>("", kResponse);

  web::test::SetUpSimpleHttpServerWithSetCookies(response);

  // Load |kUrl| and check that cookie is not set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                          kResponse)]
      assertWithMatcher:grey_notNil()];
  AssertNoCookieExists();

  // Visit |kUrlWithSetCookie| to set a cookie and then load |kUrl| to check it
  // is still set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrlWithSetCookie)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                          kResponseWithSetCookie)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                          kResponse)]
      assertWithMatcher:grey_notNil()];

  AssertCookieExists(kCookie);

  // Restore the Clear Browsing Data checkmarks prefs to their default state in
  // Teardown.
  [self setTearDownHandler:^{
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();
    PrefService* preferences = browserState->GetPrefs();

    preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
    preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
    preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  }];

  // Clear all cookies.
  [self clearCookiesAndSiteData];

  // Reload and test that there are no cookies left.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::webViewContainingText(
                                          kResponse)]
      assertWithMatcher:grey_notNil()];
  AssertNoCookieExists();
  chrome_test_util::CloseAllTabs();
}

// Verifies that logging into a form on a web page allows the user to save and
// then clear a password.
- (void)testClearPasswords {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://testClearPasswords");
  // TODO(crbug.com/432596): There looks to be a bug where the save password
  // infobar is not displayed if the action is about:blank.
  responses[URL] =
      "<form method=\"POST\" action=\"dest\">"
      "Username:<input type=\"text\" name=\"username\" value=\"name\" /><br />"
      "Password:<input type=\"password\""
      "name=\"password\" value=\"pass\"/><br />"
      "<input type=\"submit\" value=\"Login\" id=\"Login\"/>"
      "</form>";
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://testClearPasswords/dest");
  responses[destinationURL] = "Logged in!";
  web::test::SetUpSimpleHttpServer(responses);

  // Enable password management.
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  bool originalPasswordManagerSavingEnabled = preferences->GetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled);
  preferences->SetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled, true);
  [self setTearDownHandler:^{
    // Restore the password management pref state.
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();
    PrefService* preferences = browserState->GetPrefs();
    preferences->SetBoolean(
        password_manager::prefs::kPasswordManagerSavingEnabled,
        originalPasswordManagerSavingEnabled);

    // Restore the Clear Browsing Data checkmarks prefs to their default state.
    preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
    preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
    preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  }];

  // Clear passwords and check that none are saved.
  [self clearPasswords];
  [self checkIfPasswordsSaved:NO];

  // Login to page and click to save password and check that its saved.
  [ChromeEarlGrey loadURL:URL];
  chrome_test_util::TapWebViewElementWithId("Login");
  [[EarlGrey selectElementWithMatcher:buttonWithAccessibilityLabelId(
                                          IDS_IOS_PASSWORD_MANAGER_SAVE_BUTTON)]
      performAction:grey_tap()];

  [self checkIfPasswordsSaved:YES];

  // Clear passwords and check that none are saved.
  [self clearPasswords];
  [self checkIfPasswordsSaved:NO];
}

// Verifies that metrics reporting works properly under possible settings of the
// preferences kMetricsReportingEnabled and kMetricsReportingWifiOnly.
- (void)testMetricsReporting {
  [self assertsMetricsPrefsForService:kMetrics];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for non-first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReporting {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    chrome_test_util::SetFirstLaunchStateTo(
        chrome_test_util::IsFirstLaunchAfterUpgrade());
  }];

  chrome_test_util::SetFirstLaunchStateTo(NO);
  [self assertsMetricsPrefsForService:kBreakpad];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReportingFirstLaunch {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    chrome_test_util::SetFirstLaunchStateTo(
        chrome_test_util::IsFirstLaunchAfterUpgrade());
  }];

  chrome_test_util::SetFirstLaunchStateTo(YES);
  [self assertsMetricsPrefsForService:kBreakpadFirstLaunch];
}

// Set a server bound certificate, clears the site data through the UI and
// checks that the certificate is deleted.
- (void)testClearCertificates {
  SetCertificate();
  // Restore the Clear Browsing Data checkmarks prefs to their default state in
  // Teardown.
  [self setTearDownHandler:^{
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();
    PrefService* preferences = browserState->GetPrefs();

    preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
    preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
    preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
    preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  }];
  GREYAssertFalse(IsCertificateCleared(), @"Failed to set certificate.");
  [self clearCookiesAndSiteData];
  GREYAssertTrue(IsCertificateCleared(),
                 @"Certificate is expected to be deleted.");
}

// Verifies that Settings opens when signed-out and in Incognito mode.
// This tests that crbug.com/607335 has not regressed.
- (void)testSettingsSignedOutIncognito {
  chrome_test_util::OpenNewIncognitoTab();

  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsCollectionViewId)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:navigationDoneButton()]
      performAction:grey_tap()];
  chrome_test_util::CloseAllIncognitoTabs();
}

// Verifies the UI elements are accessible on the Settings page.
- (void)testAccessibilityOnSettingsPage {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::buttonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

// Verifies the UI elements are accessible on the Content Settings page.
- (void)testAccessibilityOnContentSettingsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_IOS_CONTENT_SETTINGS_TITLE)];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Content Settings
// Block Popups page.
- (void)testAccessibilityOnContentSettingsBlockPopupsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_IOS_CONTENT_SETTINGS_TITLE)];
  [[EarlGrey selectElementWithMatcher:BlockPopupsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Content Translations Settings
// page.
- (void)testAccessibilityOnContentSettingsTranslatePage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_IOS_CONTENT_SETTINGS_TITLE)];
  [[EarlGrey selectElementWithMatcher:TranslateSettingsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Settings page.
- (void)testAccessibilityOnPrivacySettingsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Handoff Settings
// page.
- (void)testAccessibilityOnPrivacyHandoffSettingsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
  [[EarlGrey selectElementWithMatcher:PrivacyHandoffButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Clear Browsing Data
// Settings page.
- (void)testAccessibilityOnPrivacyClearBrowsingHistoryPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Management Settings
// page.
- (void)testAccessibilityOnBandwidthManagementSettingsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Preload Webpages
// Settings page.
- (void)testAccessibilityOnBandwidthPreloadWebpagesSettingsPage {
  [self openSubSettingMenu:chrome_test_util::buttonWithAccessibilityLabelId(
                               IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS)];
  [[EarlGrey selectElementWithMatcher:BandwidthPreloadWebpagesButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Save Passwords page.
- (void)testAccessibilityOnSavePasswords {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:passwordsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Search engine page.
- (void)testAccessibilityOnSearchEngine {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:searchEngineButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Autofill Forms page.
- (void)testAccessibilityOnAutofillForms {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:autofillButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Google Apps page.
- (void)testAccessibilityOnGoogleApps {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:googleAppsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the About Chrome page.
- (void)testAccessibilityOnGoogleChrome {
  [self openSubSettingMenu:googleChromeButton()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Voice Search page.
- (void)testAccessibilityOnVoiceSearch {
  [self openSubSettingMenu:voiceSearchButton()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Passwords page.
- (void)testAccessibilityOnPasswords {
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:settingsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:passwordsButton()]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [self closeSubSettingsMenu];
}

@end
