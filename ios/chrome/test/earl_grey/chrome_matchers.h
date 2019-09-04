// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#import <Foundation/Foundation.h>

#include <string>

@protocol GREYMatcher;

namespace chrome_test_util {

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label);

// Matcher for element with an image corresponding to |image_id|.
id<GREYMatcher> ImageViewWithImage(int image_id);

// Matcher for element with an image defined by its name in the main bundle.
id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName);

// Matcher for element with an image corresponding to |image_id| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithImage(int image_id);

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label);

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label);

// Returns matcher for a cancel button.
id<GREYMatcher> CancelButton();

// Returns matcher for a close button.
id<GREYMatcher> CloseButton();

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

// Returns a matcher for the location view.
id<GREYMatcher> DefocusedLocationView();

// Returns a matcher for the page security info button.
id<GREYMatcher> PageSecurityInfoButton();
// Returns a matcher for the page security info indicator.
id<GREYMatcher> PageSecurityInfoIndicator();

// Returns matcher for omnibox containing |text|. Performs an exact match of the
// omnibox contents.
id<GREYMatcher> OmniboxText(const std::string& text);

// Returns matcher for |text| being a substring of the text in the omnibox.
id<GREYMatcher> OmniboxContainingText(const std::string& text);

// Returns matcher for |text| being a substring of the text in the location
// view.
id<GREYMatcher> LocationViewContainingText(const std::string& text);

// Matcher for Tools menu button.
id<GREYMatcher> ToolsMenuButton();

// Matcher for the Share menu button.
id<GREYMatcher> ShareButton();

// Returns the GREYMatcher for the button that opens the tab switcher.
id<GREYMatcher> TabletTabSwitcherOpenButton();

// Matcher for show tabs button.
id<GREYMatcher> ShowTabsButton();

// Matcher for SettingsSwitchCell.
id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on);

// Matcher for SettingsSwitchCell.
id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on,
                                   BOOL is_enabled);

// Matcher for LegacySyncSwitchCell.
id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on);

// Matcher for the Open in New Tab option in the context menu when long pressing
// a link.
id<GREYMatcher> OpenLinkInNewTabButton();

// Matcher for the done button on the navigation bar.
id<GREYMatcher> NavigationBarDoneButton();

// Matcher for the done button on the Bookmarks navigation bar.
id<GREYMatcher> BookmarksNavigationBarDoneButton();

// Returns matcher for the account consistency setup signin button.
id<GREYMatcher> AccountConsistencySetupSigninButton();

// Returns matcher for the account consistency confirmation button.
id<GREYMatcher> AccountConsistencyConfirmationOkButton();

// Returns matcher for "ADD ACCOUNT" button in unified consent dialog.
id<GREYMatcher> UnifiedConsentAddAccountButton();

// Returns matcher for the add account accounts button.
id<GREYMatcher> AddAccountButton();

// Returns matcher for the sign out accounts button.
id<GREYMatcher> SignOutAccountsButton();

// Returns matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell();

// Returns matcher for the clear browsing data button on the clear browsing data
// panel.
id<GREYMatcher> ClearBrowsingDataButton();

// Returns matcher for the clear browsing data view.
id<GREYMatcher> ClearBrowsingDataView();

// Matcher for the clear browsing data action sheet item.
id<GREYMatcher> ConfirmClearBrowsingDataButton();

// Returns matcher for the settings button in the tools menu.
id<GREYMatcher> SettingsMenuButton();

// Returns matcher for the "Done" button in the settings' navigation bar.
id<GREYMatcher> SettingsDoneButton();

// Returns matcher for the "Confirm" button in the Sync and Google Services
// settings' navigation bar.
id<GREYMatcher> SyncSettingsConfirmButton();

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

// Returns matcher for the Keep Data Separate cell in switch sync account view.
id<GREYMatcher> SettingsImportDataContinueButton();

// Returns matcher for the privacy settings table view.
id<GREYMatcher> SettingsPrivacyTableView();

// Returns matcher for the Manage Synced Data button in sync setting view.
id<GREYMatcher> SettingsSyncManageSyncedDataButton();

// Returns matcher for the menu button to sync accounts.
id<GREYMatcher> AccountsSyncButton();

// Returns matcher for the Content Settings button on the main Settings screen.
id<GREYMatcher> ContentSettingsButton();

// Returns matcher for the Google Services Settings button on the main Settings
// screen.
id<GREYMatcher> GoogleServicesSettingsButton();

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

// Returns matcher for the voice search button on the omnibox input accessory.
id<GREYMatcher> VoiceSearchInputAccessoryButton();

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

// Returns matcher for the warning message while filling in payment requests.
id<GREYMatcher> WarningMessageView();

// Returns matcher for the payment picker cell.
id<GREYMatcher> PaymentRequestPickerRow();

// Returns matcher for the payment request search bar.
id<GREYMatcher> PaymentRequestPickerSearchBar();

// Returns matcher for the bookmarks button on the Tools menu.
id<GREYMatcher> BookmarksMenuButton();

// Returns matcher for the recent tabs button on the Tools menu.
id<GREYMatcher> RecentTabsMenuButton();

// Returns matcher for the system selection callout.
id<GREYMatcher> SystemSelectionCallout();

// Returns matcher for the copy button on the system selection callout.
id<GREYMatcher> SystemSelectionCalloutCopyButton();

// Returns matcher for the Copy item on the context menu.
id<GREYMatcher> ContextMenuCopyButton();

// Returns matcher for defoucesed omnibox on a new tab.
id<GREYMatcher> NewTabPageOmnibox();

// Returns matcher for a fake omnibox on a new tab page.
id<GREYMatcher> FakeOmnibox();

// Returns a matcher for the current WebView.
id<GREYMatcher> WebViewMatcher();

// Returns a matcher for the current WebState's scroll view.
id<GREYMatcher> WebStateScrollViewMatcher();

// Returns a matcher for the Clear Browsing Data button in the History UI.
id<GREYMatcher> HistoryClearBrowsingDataButton();

// Returns a matcher for "Open In..." button.
id<GREYMatcher> OpenInButton();

// Returns the GREYMatcher for the button that opens the tab grid.
id<GREYMatcher> TabGridOpenButton();

// Returns the GREYMatcher for the cell at |index| in the tab grid.
id<GREYMatcher> TabGridCellAtIndex(unsigned int index);

// Returns the GREYMatcher for the button that closes the tab grid.
id<GREYMatcher> TabGridDoneButton();

// Returns the GREYMatcher for the button that closes all the tabs in the tab
// grid.
id<GREYMatcher> TabGridCloseAllButton();

// Returns the GREYMatcher for the button that reverts the close all tabs action
// in the tab grid.
id<GREYMatcher> TabGridUndoCloseAllButton();

// Returns the GREYMatcher for the cell that opens History in Recent Tabs.
id<GREYMatcher> TabGridSelectShowHistoryCell();

// Returns the GREYMatcher for the regular tabs empty state view.
id<GREYMatcher> TabGridRegularTabsEmptyStateView();

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab grid.
id<GREYMatcher> TabGridNewTabButton();

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab grid.
id<GREYMatcher> TabGridNewIncognitoTabButton();

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab grid.
id<GREYMatcher> TabGridOpenTabsPanelButton();

// Returns the GREYMatcher for the button to go to the incognito panel in
// the tab grid.
id<GREYMatcher> TabGridIncognitoTabsPanelButton();

// Returns the GREYMatcher for the button to go to the other devices panel in
// the tab grid.
id<GREYMatcher> TabGridOtherDevicesPanelButton();

// Returns the GREYMatcher for the button to close the cell at |index| in the
// tab grid.
id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index);
}

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
