// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_assertions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

namespace {

// URLs used in the tests.
const char* kBlockPopupsUrl = "http://blockpopups";
const char* kOpenedWindowUrl = "http://openedwindow";

// JavaScript to open a new window after a short delay.
NSString* kBlockPopupsResponseTemplate =
    @"<input type=\"button\" onclick=\"setTimeout(function() {"
     "window.open('%@')}, 1)\" "
     "id=\"openWindow\" "
     "value=\"openWindow\">";
const std::string kOpenedWindowResponse = "Opened window";

// Opens the block popups settings page.  Must be called from the NTP.
void OpenBlockPopupsSettings() {
  const CGFloat scroll_displacement = 50.0;
  id<GREYMatcher> tools_menu_table_view_matcher =
      grey_accessibilityID(kToolsMenuTableViewId);
  id<GREYMatcher> settings_button_matcher =
      grey_accessibilityID(kToolsMenuSettingsId);
  id<GREYMatcher> content_settings_button_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_CONTENT_SETTINGS_TITLE);
  id<GREYMatcher> settings_collection_view_matcher =
      grey_accessibilityID(kSettingsCollectionViewId);
  id<GREYMatcher> block_popups_button_matcher =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_IOS_BLOCK_POPUPS);

  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey selectElementWithMatcher:settings_button_matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:tools_menu_table_view_matcher]
      performAction:grey_tap()];

  [[[EarlGrey selectElementWithMatcher:content_settings_button_matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:settings_collection_view_matcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:block_popups_button_matcher]
      performAction:grey_tap()];
}

// Exits out of settings.  Must be called from the block popups settings page.
void CloseSettings() {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];
}

// ScopedBlockPopupsPref modifies the block popups preference and resets the
// preference to its original value when this object goes out of scope.
class ScopedBlockPopupsPref {
 public:
  ScopedBlockPopupsPref(ContentSetting setting) {
    original_setting_ = GetPrefValue();
    SetPrefValue(setting);
  }
  ~ScopedBlockPopupsPref() { SetPrefValue(original_setting_); }

 private:
  // Gets the current value of the preference.
  ContentSetting GetPrefValue() {
    ContentSetting popupSetting =
        ios::HostContentSettingsMapFactory::GetForBrowserState(
            chrome_test_util::GetOriginalBrowserState())
            ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, NULL);
    return popupSetting;
  }

  // Sets the preference to the given value.
  void SetPrefValue(ContentSetting setting) {
    DCHECK(setting == CONTENT_SETTING_BLOCK ||
           setting == CONTENT_SETTING_ALLOW);
    ios::ChromeBrowserState* state =
        chrome_test_util::GetOriginalBrowserState();
    ios::HostContentSettingsMapFactory::GetForBrowserState(state)
        ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS, setting);
  }

  // Saves the original pref setting so that it can be restored when the scoper
  // is destroyed.
  ContentSetting original_setting_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockPopupsPref);
};

// ScopedBlockPopupsException adds an exception to the block popups exception
// list for as long as this object is in scope.
class ScopedBlockPopupsException {
 public:
  ScopedBlockPopupsException(const std::string& pattern) : pattern_(pattern) {
    SetException(pattern_, CONTENT_SETTING_ALLOW);
  }
  ~ScopedBlockPopupsException() {
    SetException(pattern_, CONTENT_SETTING_DEFAULT);
  }

 private:
  // Adds an exception for the given |pattern|.  If |setting| is
  // CONTENT_SETTING_DEFAULT, removes the existing exception instead.
  void SetException(const std::string& pattern, ContentSetting setting) {
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();

    ContentSettingsPattern exception_pattern =
        ContentSettingsPattern::FromString(pattern);
    ios::HostContentSettingsMapFactory::GetForBrowserState(browserState)
        ->SetContentSettingCustomScope(
            exception_pattern, ContentSettingsPattern::Wildcard(),
            CONTENT_SETTINGS_TYPE_POPUPS, std::string(), setting);
  }

  // The exception pattern that this object is managing.
  std::string pattern_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBlockPopupsException);
};
}  // namespace

// Block Popups tests for Chrome.
@interface BlockPopupsTestCase : ChromeTestCase
@end

@implementation BlockPopupsTestCase

// Opens the block popups settings page and verifies that accessibility is set
// up properly.
- (void)testAccessibilityOfBlockPopupSettings {
  OpenBlockPopupsSettings();
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"block_popups_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  CloseSettings();
}

// Tests that popups are opened in new tabs when the preference is set to ALLOW.
- (void)testPopupsAllowed {
  std::map<GURL, std::string> responses;
  const GURL blockPopupsURL = web::test::HttpServer::MakeUrl(kBlockPopupsUrl);
  const GURL openedWindowURL = web::test::HttpServer::MakeUrl(kOpenedWindowUrl);
  NSString* openedWindowURLString =
      base::SysUTF8ToNSString(openedWindowURL.spec());
  responses[blockPopupsURL] = base::SysNSStringToUTF8([NSString
      stringWithFormat:kBlockPopupsResponseTemplate, openedWindowURLString]);
  responses[openedWindowURL] = kOpenedWindowResponse;
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);
  [ChromeEarlGrey loadURL:blockPopupsURL];
  chrome_test_util::AssertMainTabCount(1U);

  // Tap the "Open Window" link and make sure the popup opened in a new tab.
  chrome_test_util::TapWebViewElementWithId("openWindow");
  chrome_test_util::AssertMainTabCount(2U);

  // No infobar should be displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              @"Pop-ups blocked (1)")]
      assertWithMatcher:grey_notVisible()];
}

// Tests that popups are prevented from opening and an infobar is displayed when
// the preference is set to BLOCK.
- (void)testPopupsBlocked {
  std::map<GURL, std::string> responses;
  const GURL blockPopupsURL = web::test::HttpServer::MakeUrl(kBlockPopupsUrl);
  const GURL openedWindowURL = web::test::HttpServer::MakeUrl(kOpenedWindowUrl);
  NSString* openedWindowURLString =
      base::SysUTF8ToNSString(openedWindowURL.spec());
  responses[blockPopupsURL] = base::SysNSStringToUTF8([NSString
      stringWithFormat:kBlockPopupsResponseTemplate, openedWindowURLString]);
  responses[openedWindowURL] = kOpenedWindowResponse;
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  [ChromeEarlGrey loadURL:blockPopupsURL];
  chrome_test_util::AssertMainTabCount(1U);

  // Tap the "Open Window" link, then make sure the popup was blocked and an
  // infobar was displayed.  The window.open() call is run via async JS, so the
  // infobar may not open immediately.
  chrome_test_util::TapWebViewElementWithId("openWindow");
  [[GREYCondition
      conditionWithName:@"Wait for blocked popups infobar to show"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:
                            chrome_test_util::StaticTextWithAccessibilityLabel(
                                @"Pop-ups blocked (1)")]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:4.0];
  chrome_test_util::AssertMainTabCount(1U);
}

// Tests that the "exceptions" section on the settings page is hidden and
// revealed properly when the preference switch is toggled.
- (void)testSettingsPageWithExceptions {
  std::string allowedPattern = "[*.]example.com";
  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_BLOCK);
  ScopedBlockPopupsException exceptionSetter(allowedPattern);

  OpenBlockPopupsSettings();

  // Make sure that the "example.com" exception is listed.
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle the switch off via the UI and make sure the exceptions are not
  // visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::CollectionViewSwitchCell(
                                   @"blockPopupsContentView_switch", YES)]
      performAction:chrome_test_util::turnCollectionViewSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle the switch back on via the UI and make sure the exceptions are now
  // visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::CollectionViewSwitchCell(
                                   @"blockPopupsContentView_switch", NO)]
      performAction:chrome_test_util::turnCollectionViewSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:grey_text(base::SysUTF8ToNSString(
                                          allowedPattern))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  CloseSettings();
}

@end
