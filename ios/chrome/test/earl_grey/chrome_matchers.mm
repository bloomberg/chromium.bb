// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#import <EarlGrey/EarlGrey.h>
#import <WebKit/WebKit.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_picker_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
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

id<GREYMatcher> SettingsSwitchIsToggledOn(BOOL isToggledOn) {
  MatchesBlock matches = ^BOOL(id element) {
    SettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && isToggledOn) || (!switch_view.on && !isToggledOn);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchToggledState(%@)",
                                   isToggledOn ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> SettingsSwitchIsEnabled(BOOL isEnabled) {
  MatchesBlock matches = ^BOOL(id element) {
    SettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.enabled && isEnabled) ||
           (!switch_view.enabled && !isEnabled);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchEnabledState(%@)",
                                   isEnabled ? @"YES" : @"NO"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns the subview of |parentView| corresponding to the
// ContentSuggestionsViewController. Returns nil if it is not in its subviews.
UIView* SubviewWithAccessibilityIdentifier(NSString* accessibilityID,
                                           UIView* parentView) {
  if (parentView.accessibilityIdentifier == accessibilityID) {
    return parentView;
  }
  for (UIView* view in parentView.subviews) {
    UIView* resultView =
        SubviewWithAccessibilityIdentifier(accessibilityID, view);
    if (resultView)
      return resultView;
  }
  return nil;
}

}  // namespace

@implementation ChromeMatchers

+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabelId:(int)messageId {
  return [ChromeMatchers
      buttonWithAccessibilityLabel:l10n_util::GetNSStringWithFixup(messageId)];
}

+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName {
  UIImage* expectedImage = [UIImage imageNamed:imageName];
  MatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching image named %@", imageName];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)imageViewWithImage:(int)imageId {
  UIImage* expectedImage = NativeImage(imageId);
  MatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageId];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)buttonWithImage:(int)imageId {
  UIImage* expectedImage = NativeImage(imageId);
  MatchesBlock matches = ^BOOL(UIButton* button) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     [button currentImage]);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageId];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    imageMatcher, nil);
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabelId:(int)messageId {
  return [ChromeMatchers
      staticTextWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                           messageId))];
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

+ (id<GREYMatcher>)headerWithAccessibilityLabelId:(int)messageId {
  return [ChromeMatchers
      headerWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                       messageId))];
}

+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
}

+ (id<GREYMatcher>)cancelButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_CANCEL)];
}

+ (id<GREYMatcher>)closeButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_CLOSE)];
}

+ (id<GREYMatcher>)forwardButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_ACCNAME_FORWARD)];
}

+ (id<GREYMatcher>)backButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_ACCNAME_BACK)];
}

+ (id<GREYMatcher>)reloadButton {
  return
      [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_IOS_ACCNAME_RELOAD)];
}

+ (id<GREYMatcher>)stopButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_IOS_ACCNAME_STOP)];
}

+ (id<GREYMatcher>)omnibox {
  return grey_kindOfClass([OmniboxTextFieldIOS class]);
}

+ (id<GREYMatcher>)defocusedLocationView {
  return grey_kindOfClass([LocationBarSteadyView class]);
}

+ (id<GREYMatcher>)pageSecurityInfoButton {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)pageSecurityInfoIndicator {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)omniboxText:(std::string)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::mac::ObjCCast<OmniboxTextFieldIOS>(element);
        return [omnibox.text isEqualToString:base::SysUTF8ToNSString(text)];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:@"Omnibox contains text \"%@\"",
                                            base::SysUTF8ToNSString(text)]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)omniboxContainingText:(std::string)text {
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

+ (id<GREYMatcher>)locationViewContainingText:(std::string)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(LocationBarSteadyView* element) {
        return [element.locationLabel.text
            containsString:base::SysUTF8ToNSString(text)];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:
                               @"LocationBarSteadyView contains text \"%@\"",
                               base::SysUTF8ToNSString(text)]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)toolsMenuButton {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)shareButton {
  return grey_allOf(
      [ChromeMatchers
          buttonWithAccessibilityLabelId:(IDS_IOS_TOOLS_MENU_SHARE)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabletTabSwitcherOpenButton {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER)];
}

+ (id<GREYMatcher>)showTabsButton {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn {
  return [ChromeMatchers settingsSwitchCell:accessibilityIdentifier
                                isToggledOn:isToggledOn
                                  isEnabled:YES];
}

+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn
                            isEnabled:(BOOL)isEnabled {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    SettingsSwitchIsToggledOn(isToggledOn),
                    SettingsSwitchIsEnabled(isEnabled),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)syncSwitchCell:(NSString*)accessibilityLabel
                      isToggledOn:(BOOL)isToggledOn {
  return grey_allOf(
      grey_accessibilityLabel(accessibilityLabel),
      grey_accessibilityValue(
          isToggledOn ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                      : l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)openLinkInNewTabButton {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)];
}

+ (id<GREYMatcher>)navigationBarDoneButton {
  return grey_allOf(
      [ChromeMatchers
          buttonWithAccessibilityLabelId:(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)],
      grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

+ (id<GREYMatcher>)accountConsistencySetupSigninButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:
                             (IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON)];
}

+ (id<GREYMatcher>)accountConsistencyConfirmationOkButton {
  int labelID = base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent)
                    ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON
                    : IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON;
  return [ChromeMatchers buttonWithAccessibilityLabelId:(labelID)];
}

+ (id<GREYMatcher>)addAccountButton {
  return grey_accessibilityID(kSettingsAccountsTableViewAddAccountCellId);
}

+ (id<GREYMatcher>)signOutAccountsButton {
  return grey_accessibilityID(kSettingsAccountsTableViewSignoutCellId);
}

+ (id<GREYMatcher>)clearBrowsingDataCell {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)];
}

+ (id<GREYMatcher>)clearBrowsingDataButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_IOS_CLEAR_BUTTON)];
}

+ (id<GREYMatcher>)clearBrowsingDataCollectionView {
  return grey_accessibilityID(
      kClearBrowsingDataCollectionViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)confirmClearBrowsingDataButton {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityID(kClearBrowsingDataButtonIdentifier)), nil);
}

+ (id<GREYMatcher>)settingsMenuButton {
  return grey_accessibilityID(kToolsMenuSettingsId);
}

+ (id<GREYMatcher>)settingsDoneButton {
  return grey_accessibilityID(kSettingsDoneButtonId);
}

+ (id<GREYMatcher>)toolsMenuView {
  return grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
}

+ (id<GREYMatcher>)okButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_OK)];
}

+ (id<GREYMatcher>)primarySignInButton {
  return grey_accessibilityID(kSigninPromoPrimaryButtonId);
}

+ (id<GREYMatcher>)secondarySignInButton {
  return grey_accessibilityID(kSigninPromoSecondaryButtonId);
}

+ (id<GREYMatcher>)settingsAccountButton {
  return grey_accessibilityID(kSettingsAccountCellId);
}

+ (id<GREYMatcher>)settingsAccountsCollectionView {
  return grey_accessibilityID(kSettingsAccountsTableViewId);
}

+ (id<GREYMatcher>)settingsImportDataImportButton {
  return grey_accessibilityID(kImportDataImportCellId);
}

+ (id<GREYMatcher>)settingsImportDataKeepSeparateButton {
  return grey_accessibilityID(kImportDataKeepSeparateCellId);
}

+ (id<GREYMatcher>)settingsSyncManageSyncedDataButton {
  return grey_accessibilityID(kSettingsSyncId);
}

+ (id<GREYMatcher>)accountsSyncButton {
  return grey_allOf(grey_accessibilityID(kSettingsAccountsTableViewSyncCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)contentSettingsButton {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_CONTENT_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)googleServicesSettingsButton {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)settingsMenuBackButton {
  UINavigationBar* navBar = base::mac::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(
          @"SettingNavigationBar",
          [[UIApplication sharedApplication] keyWindow]));
  return grey_allOf(grey_anyOf(grey_accessibilityLabel(navBar.backItem.title),
                               grey_accessibilityLabel(@"Back"), nil),
                    grey_kindOfClass([UIButton class]),
                    grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                    nil);
}

+ (id<GREYMatcher>)settingsMenuPrivacyButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:
                             (IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
}

+ (id<GREYMatcher>)settingsMenuPasswordsButton {
  return [ChromeMatchers buttonWithAccessibilityLabelId:(IDS_IOS_PASSWORDS)];
}

+ (id<GREYMatcher>)paymentRequestView {
  return grey_accessibilityID(kPaymentRequestCollectionViewID);
}

// Returns matcher for the error confirmation view for payment request.
+ (id<GREYMatcher>)paymentRequestErrorView {
  return grey_accessibilityID(kPaymentRequestErrorCollectionViewID);
}

+ (id<GREYMatcher>)voiceSearchButton {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)settingsCollectionView {
  return grey_accessibilityID(kSettingsTableViewId);
}

+ (id<GREYMatcher>)clearBrowsingHistoryButton {
  return grey_accessibilityID(kClearBrowsingHistoryCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)clearCookiesButton {
  return grey_accessibilityID(kClearCookiesCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)clearCacheButton {
  return grey_accessibilityID(kClearCacheCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)clearSavedPasswordsButton {
  return grey_accessibilityID(kClearSavedPasswordsCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)contentSuggestionCollectionView {
  return grey_accessibilityID(
      [ContentSuggestionsViewController collectionAccessibilityIdentifier]);
}

+ (id<GREYMatcher>)warningMessageView {
  return grey_accessibilityID(kWarningMessageAccessibilityID);
}

+ (id<GREYMatcher>)paymentRequestPickerRow {
  return grey_accessibilityID(kPaymentRequestPickerRowAccessibilityID);
}

+ (id<GREYMatcher>)paymentRequestPickerSearchBar {
  return grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID);
}

+ (id<GREYMatcher>)bookmarksMenuButton {
  return grey_accessibilityID(kToolsMenuBookmarksId);
}

+ (id<GREYMatcher>)recentTabsMenuButton {
  return grey_accessibilityID(kToolsMenuOtherDevicesId);
}

+ (id<GREYMatcher>)systemSelectionCallout {
  return grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"));
}

+ (id<GREYMatcher>)systemSelectionCalloutCopyButton {
  return grey_accessibilityLabel(@"Copy");
}

+ (id<GREYMatcher>)contextMenuCopyButton {
  return [ChromeMatchers
      buttonWithAccessibilityLabelId:(IDS_IOS_CONTENT_CONTEXT_COPY)];
}

+ (id<GREYMatcher>)NTPOmnibox {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
}

+ (id<GREYMatcher>)webViewMatcher {
  return web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
}

@end
