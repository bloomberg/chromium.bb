// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

namespace chrome_test_util {

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label);

// Matcher for element with an image corresponding to |image_id| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithImage(int image_id);

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label);

// Returns matcher for a cancel button.
id<GREYMatcher> CancelButton();

// Matcher for the navigate forward button.
id<GREYMatcher> ForwardButton();

// Matcher for the navigate backward button.
id<GREYMatcher> BackButton();

// Matcher for the reload button.
id<GREYMatcher> ReloadButton();

// Matcher for the stop loading button.
id<GREYMatcher> StopButton();

// Returns a matcher for the omnibox.
id<GREYMatcher> Omnibox();

// Returns a matcher for the page security info button.
id<GREYMatcher> PageSecurityInfoButton();

// Returns matcher for omnibox containing |text|. Performs an exact match of the
// omnibox contents.
id<GREYMatcher> OmniboxText(std::string text);

// Returns matcher for |text| being a substring of the text in the omnibox.
id<GREYMatcher> OmniboxContainingText(std::string text);

// Matcher for Tools menu button.
id<GREYMatcher> ToolsMenuButton();

// Matcher for the Share menu button.
id<GREYMatcher> ShareButton();

// Matcher for show tabs button.
id<GREYMatcher> ShowTabsButton();

// Matcher for CollectionViewSwitchCell.
// TODO(crbug.com/684139): Update |isOn| to something more obvious from
// callsites.
id<GREYMatcher> CollectionViewSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isOn);

// Matcher for the Open in New Tab option in the context menu when long pressing
// a link.
id<GREYMatcher> OpenLinkInNewTabButton();

// Matcher for the done button on the navigation bar.
id<GREYMatcher> NavigationBarDoneButton();

// Returns matcher for the account consistency setup signin button.
id<GREYMatcher> AccountConsistencySetupSigninButton();

// Returns matcher for the account consistency confirmation button.
id<GREYMatcher> AccountConsistencyConfirmationOkButton();

// Returns matcher for the sign out accounts button.
id<GREYMatcher> SignOutAccountsButton();

// Returns matcher for the clear browsing data collection view.
id<GREYMatcher> ClearBrowsingDataCollectionView();

// Returns matcher for the settings button in the tools menu.
id<GREYMatcher> SettingsMenuButton();

// Returns matcher for the tools menu table view.
id<GREYMatcher> ToolsMenuView();

// Returns matcher for the OK button.
id<GREYMatcher> OKButton();

// Returns matcher for the primary button in the sign-in promo view. This is
// "Sign in into Chrome" button for a cold state, or "Continue as John Doe" for
// a warm state.
id<GREYMatcher> PrimarySignInButton();

// Returns matcher for the secondary button in the sign-in promo view. This is
// "Not johndoe@example.com" button.
id<GREYMatcher> SecondarySignInButton();

// Returns matcher for the button for the currently signed in account in the
// settings menu.
id<GREYMatcher> SettingsAccountButton();

// Returns matcher for the accounts collection view.
id<GREYMatcher> SettingsAccountsCollectionView();

// Returns matcher for the Import Data cell in switch sync account view.
id<GREYMatcher> SettingsImportDataImportButton();

// Returns matcher for the Keep Data Separate cell in switch sync account view.
id<GREYMatcher> SettingsImportDataKeepSeparateButton();

// Returns matcher for the Manage Synced Data button in sync setting view.
id<GREYMatcher> SettingsSyncManageSyncedDataButton();

// Returns matcher for the menu button to sync accounts.
id<GREYMatcher> AccountsSyncButton();

// Returns matcher for the Content Settings button on the main Settings screen.
id<GREYMatcher> ContentSettingsButton();

// Returns matcher for the back button on a settings menu.
id<GREYMatcher> SettingsMenuBackButton();

// Returns matcher for the Privacy cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPrivacyButton();

// Returns matcher for the Save passwords cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPasswordsButton();

// Returns matcher for the payment request collection view.
id<GREYMatcher> PaymentRequestView();

// Returns matcher for the error confirmation view for payment request.
id<GREYMatcher> PaymentRequestErrorView();

// Returns matcher for the voice search button on the main Settings screen.
id<GREYMatcher> VoiceSearchButton();

// Returns matcher for the settings main menu view.
id<GREYMatcher> SettingsCollectionView();

// Returns matcher for the clear browsing history cell on the clear browsing
// data panel.
id<GREYMatcher> ClearBrowsingHistoryButton();

// Returns matcher for the clear cookies cell on the clear browsing data panel.
id<GREYMatcher> ClearCookiesButton();

// Returns matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> ClearCacheButton();

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
id<GREYMatcher> ClearSavedPasswordsButton();

// Returns matcher for the collection view of content suggestion.
id<GREYMatcher> ContentSuggestionCollectionView();
}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
