// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_

namespace flag_descriptions {

// Title and description for the flag to controll the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to controll the dynamic autofill.
extern const char kAutofillDynamicFormsName[];
extern const char kAutofillDynamicFormsDescription[];

// Title and description for the flag to restrict extraction of formless forms
// to checkout flows.
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

// Title and description for the flag to control redirection to the task
// scheduler.
extern const char kBrowserTaskScheduler[];
extern const char kBrowserTaskSchedulerDescription[];

// Title and description for the flag to enable Captive Portal Login.
extern const char kCaptivePortalName[];
extern const char kCaptivePortalDescription[];

// Title and description for the flag to enable Captive Portal metrics logging.
extern const char kCaptivePortalMetricsName[];
extern const char kCaptivePortalMetricsDescription[];

// Title and description for the flag to enable Contextual Search.
extern const char kContextualSearch[];
extern const char kContextualSearchDescription[];

// Title and description for the flag to enable returning the DOM element for
// context menu using webkit postMessage API.
extern const char kContextMenuElementPostMessageName[];
extern const char kContextMenuElementPostMessageDescription[];

// Title and description for the flag to enable drag and drop.
extern const char kDragAndDropName[];
extern const char kDragAndDropDescription[];

// Title and description for the flag to enable new Clear Browsing Data UI.
extern const char kNewClearBrowsingDataUIName[];
extern const char kNewClearBrowsingDataUIDescription[];

// Title and description for the flag to enable External Search.
extern const char kExternalSearchName[];
extern const char kExternalSearchDescription[];

// Title and description for the flags to enable use of FeedbackKit V2.
extern const char kFeedbackKitV2Name[];
extern const char kFeedbackKitV2Description[];
extern const char kFeedbackKitV2WithSSOServiceName[];
extern const char kFeedbackKitV2WithSSOServiceDescription[];

// Title and description for the flag used to enable fullscreen's contentInset
// based implementation.
extern const char kFullscreenContentInsetName[];
extern const char kFullscreenContentInsetDescription[];

// Title and description for the flag to enable History batch filtering.
extern const char kHistoryBatchUpdatesFilterName[];
extern const char kHistoryBatchUpdatesFilterDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable ITunes links store kit handling.
extern const char kITunesLinksStoreKitHandlingName[];
extern const char kITunesLinksStoreKitHandlingDescription[];

// Title, description, and options for Google UI menu for handling mailto links.
extern const char kMailtoHandlingWithGoogleUIName[];
extern const char kMailtoHandlingWithGoogleUIDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];

// Title and description for the flag to enable the Memex Tab Switcher.
extern const char kMemexTabSwitcherName[];
extern const char kMemexTabSwitcherDescription[];

// Title and description for the flag to enable new tools menu.
extern const char kNewToolsMenuName[];
extern const char kNewToolsMenuDescription[];

// Title and description for the flag to enable elision of the URL path, query,
// and ref in omnibox URL suggestions.
extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

// Title and description for the flag to enable the ability to export passwords
// from the password settings.
extern const char kPasswordExportName[];
extern const char kPasswordExportDescription[];

// Title and description for the flag to enable Physical Web in the omnibox.
extern const char kPhysicalWeb[];
extern const char kPhysicalWebDescription[];

// Title and description for the flag to enable the new UI Reboot on existing
// Collections.
extern const char kCollectionsUIRebootName[];
extern const char kCollectionsUIRebootDescription[];

// Title and description for the flag to enable WKBackForwardList based
// navigation manager.
extern const char kSlimNavigationManagerName[];
extern const char kSlimNavigationManagerDescription[];

// Title and description for the flag to enable new Download Manager UI and
// backend.
extern const char kNewFileDownloadName[];
extern const char kNewFileDownloadDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to enable the TabSwitcher to present the
// BVC.
extern const char kTabSwitcherPresentsBVCName[];
extern const char kTabSwitcherPresentsBVCDescription[];

// Title and description for the flag to enable the UI Refresh location bar.
extern const char kUIRefreshLocationBarName[];
extern const char kUIRefreshLocationBarDescription[];

// Title and description for the flag to enable the phase 1 UI Refresh.
extern const char kUIRefreshPhase1Name[];
extern const char kUIRefreshPhase1Description[];

// Title and description for the flag to enable the unified consent.
extern const char kUnifiedConsentName[];
extern const char kUnifiedConsentDescription[];

// Title and description for the flag to enable the ddljson Doodle API.
extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

// Title and description for the flag to enable Web Payments.
extern const char kWebPaymentsName[];
extern const char kWebPaymentsDescription[];

// Title and description for the flag to enable third party payment app
// integration with Web Payments.
extern const char kWebPaymentsNativeAppsName[];
extern const char kWebPaymentsNativeAppsDescription[];

// Title and description for the flag to enable WKHTTPSystemCookieStore usage
// for main context URL requests.
extern const char kWKHTTPSystemCookieStoreName[];
extern const char kWKHTTPSystemCookieStoreDescription[];

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
