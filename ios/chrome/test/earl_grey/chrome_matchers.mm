// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#import <OCHamcrest/OCHamcrest.h>

#import <WebKit/WebKit.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/import_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/block_types.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

id<GREYMatcher> CollectionViewSwitchIsOn(BOOL is_on) {
  MatchesBlock matches = ^BOOL(id element) {
    CollectionViewSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<CollectionViewSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && is_on) || (!switch_view.on && !is_on);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"collectionViewSwitchInState(%@)",
                                   is_on ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

}  // namespace

namespace chrome_test_util {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> ButtonWithImage(int image_id) {
  UIImage* expected_image = NativeImage(image_id);
  MatchesBlock matches = ^BOOL(UIButton* button) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expected_image,
                                                     [button currentImage]);
  };
  NSString* description_string =
      [NSString stringWithFormat:@"Images matching %i", image_id];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:description_string];
  };
  id<GREYMatcher> image_matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    image_matcher, nil);
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return StaticTextWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> WebViewContainingBlockedImage(std::string image_id) {
  return web::WebViewContainingBlockedImage(
      std::move(image_id), chrome_test_util::GetCurrentWebState());
}

id<GREYMatcher> WebViewContainingLoadedImage(std::string image_id) {
  return web::WebViewContainingLoadedImage(
      std::move(image_id), chrome_test_util::GetCurrentWebState());
}

id<GREYMatcher> CancelButton() {
  return ButtonWithAccessibilityLabelId(IDS_CANCEL);
}

id<GREYMatcher> ForwardButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_FORWARD);
}

id<GREYMatcher> BackButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_BACK);
}

id<GREYMatcher> ReloadButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
}

id<GREYMatcher> StopButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_STOP);
}

id<GREYMatcher> Omnibox() {
  return grey_kindOfClass([OmniboxTextFieldIOS class]);
}

id<GREYMatcher> PageSecurityInfoButton() {
  return grey_accessibilityLabel(@"Page Security Info");
}

id<GREYMatcher> OmniboxText(std::string text) {
  return grey_allOf(Omnibox(),
                    hasProperty(@"text", base::SysUTF8ToNSString(text)), nil);
}

id<GREYMatcher> OmniboxContainingText(std::string text) {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(UITextField* element) {
        return [element.text containsString:base::SysUTF8ToNSString(text)];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:@"Omnibox contains text \"%@\"",
                                            base::SysUTF8ToNSString(text)]];
      }];
  return matcher;
}

id<GREYMatcher> ToolsMenuButton() {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ShareButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_SHARE);
}

id<GREYMatcher> ShowTabsButton() {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> CollectionViewSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL is_on) {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    CollectionViewSwitchIsOn(is_on), grey_sufficientlyVisible(),
                    nil);
}

id<GREYMatcher> OpenLinkInNewTabButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
}

id<GREYMatcher> NavigationBarDoneButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

id<GREYMatcher> AccountConsistencySetupSigninButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
}

id<GREYMatcher> AccountConsistencyConfirmationOkButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON);
}

id<GREYMatcher> SignOutAccountsButton() {
  return grey_accessibilityID(kSettingsAccountsSignoutCellId);
}

id<GREYMatcher> ClearBrowsingDataCollectionView() {
  return grey_accessibilityID(kClearBrowsingDataCollectionViewId);
}

id<GREYMatcher> SettingsMenuButton() {
  return grey_accessibilityID(kToolsMenuSettingsId);
}

id<GREYMatcher> ToolsMenuView() {
  return grey_accessibilityID(kToolsMenuTableViewId);
}

id<GREYMatcher> OKButton() {
  return ButtonWithAccessibilityLabelId(IDS_OK);
}

id<GREYMatcher> PrimarySignInButton() {
  return grey_accessibilityID(kSigninPromoPrimaryButtonId);
}

id<GREYMatcher> SecondarySignInButton() {
  return grey_accessibilityID(kSigninPromoSecondaryButtonId);
}

id<GREYMatcher> SettingsAccountButton() {
  return grey_accessibilityID(kSettingsAccountCellId);
}

id<GREYMatcher> SettingsAccountsCollectionView() {
  return grey_accessibilityID(kSettingsAccountsId);
}

id<GREYMatcher> SettingsImportDataImportButton() {
  return grey_accessibilityID(kImportDataImportCellId);
}

id<GREYMatcher> SettingsImportDataKeepSeparateButton() {
  return grey_accessibilityID(kImportDataKeepSeparateCellId);
}

id<GREYMatcher> SettingsSyncManageSyncedDataButton() {
  return grey_accessibilityID(kSettingsSyncId);
}

id<GREYMatcher> AccountsSyncButton() {
  return grey_accessibilityID(kSettingsAccountsSyncCellId);
}

id<GREYMatcher> ContentSettingsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_SETTINGS_TITLE);
}

id<GREYMatcher> SettingsMenuBackButton() {
  return grey_allOf(grey_accessibilityID(@"ic_arrow_back"),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> SettingsMenuPrivacyButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
}

id<GREYMatcher> SettingsMenuPasswordsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_SAVE_PASSWORDS);
}

id<GREYMatcher> PaymentRequestView() {
  return grey_accessibilityID(kPaymentRequestCollectionViewID);
}

// Returns matcher for the error confirmation view for payment request.
id<GREYMatcher> PaymentRequestErrorView() {
  return grey_accessibilityID(kPaymentRequestErrorCollectionViewID);
}

}  // namespace chrome_test_util
