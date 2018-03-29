// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kBrowserTaskScheduler[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

const char kCaptivePortalName[] = "Captive Portal";
const char kCaptivePortalDescription[] =
    "When enabled, the Captive Portal landing page will be displayed if it is "
    "detected that the user is connected to a Captive Portal network.";

const char kCaptivePortalMetricsName[] = "Captive Portal Metrics";
const char kCaptivePortalMetricsDescription[] =
    "When enabled, some network issues will trigger a test to check if a "
    "Captive Portal network is the cause of the issue.";

const char kContextualSearch[] = "Contextual Search";
const char kContextualSearchDescription[] =
    "Whether or not Contextual Search is enabled.";

const char kContextMenuElementPostMessageName[] =
    "Context Menu Element Post Message";
const char kContextMenuElementPostMessageDescription[] =
    "When enabled, the DOM element for the Context Menu is returned using a "
    "webkit postMessage call instead of directly returned from the JavaScript "
    "function.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kNewClearBrowsingDataUIName[] = "Clear Browsing Data UI";
const char kNewClearBrowsingDataUIDescription[] =
    "Enable new Clear Browsing Data UI.";

const char kExternalSearchName[] = "External Search";
const char kExternalSearchDescription[] = "Enable support for External Search.";

const char kFeedbackKitV2Name[] = "FeedbackKit V2";
const char kFeedbackKitV2Description[] = "Enable use of FeedbackKit V2.";

const char kHistoryBatchUpdatesFilterName[] = "History Single Batch Filtering";
const char kHistoryBatchUpdatesFilterDescription[] =
    "When enabled History inserts and deletes history items in the same "
    "BatchUpdates block.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kITunesLinksStoreKitHandlingName[] = "Store kit for ITunes links";
const char kITunesLinksStoreKitHandlingDescription[] =
    "When enabled, opening itunes product links will be handled using the "
    "store kit.";

const char kMailtoHandlingWithGoogleUIName[] = "Mailto Handling with Google UI";
const char kMailtoHandlingWithGoogleUIDescription[] =
    "When enabled, tapping mailto: links will open a contextual menu to allow "
    "users to select how they would like to handle the current and future "
    "mailto link interactions. This UI matches the same user experience as in "
    "other Google iOS apps.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kMemexTabSwitcherName[] = "Enable the Memex prototype Tab Switcher.";
const char kMemexTabSwitcherDescription[] =
    "When enabled, the TabSwitcher button will navigate to the chrome memex "
    "prototype site instead of triggering the native Tab Switcher. The native "
    "TabSwitcher is accessible by long pressing the button";

const char kNewToolsMenuName[] = "Enable the new tools menu";
const char kNewToolsMenuDescription[] =
    "When enabled, the new tools menu is displayed";

const char kOmniboxUIElideSuggestionUrlAfterHostName[] =
    "Hide the path, query, and ref of omnibox suggestions";
const char kOmniboxUIElideSuggestionUrlAfterHostDescription[] =
    "Elides the path, query, and ref of suggested URLs in the omnibox "
    "dropdown.";

const char kPasswordExportName[] = "Password Export";
const char kPasswordExportDescription[] =
    "Enables password exporting functionality in password settings.";

const char kPhysicalWeb[] = "Physical Web";
const char kPhysicalWebDescription[] =
    "When enabled, the omnibox will include suggestions for web pages "
    "broadcast by devices near you.";

const char kCollectionsUIRebootName[] = "Collections UI Reboot";
const char kCollectionsUIRebootDescription[] =
    "When enabled, Collections will use the new UI Reboot stack.";

const char kSlimNavigationManagerName[] = "Use Slim Navigation Manager";
const char kSlimNavigationManagerDescription[] =
    "When enabled, uses the experimental slim navigation manager that provides "
    "better compatibility with HTML navigation spec.";

const char kNewFileDownloadName[] = "Use new Download Manager UI and backend";
const char kNewFileDownloadDescription[] =
    "When enabled, uses new Download Manager UI and ios/web Download API as "
    "backend.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kTabSwitcherPresentsBVCName[] = "TabSwitcher Presents BVC";
const char kTabSwitcherPresentsBVCDescription[] =
    "When enabled, the tab switcher will present the BVC, so that when the "
    "BVC is visible, the tab switcher will remain in the VC hierarchy "
    "underneath it.";

const char kToolbarButtonPositionsName[] = "Toolbar's buttons positions";
const char kToolbarButtonPositionsDescription[] =
    "Position of the toolbars buttons.";

const char kUIRefreshPhase1Name[] = "UI Refresh Phase 1";
const char kUIRefreshPhase1Description[] =
    "When enabled, the first phase of the iOS UI refresh will be displayed.";

const char kSearchIconToggleName[] = "Change the icon for the search button";
const char kSearchIconToggleDescription[] =
    "Different icons for the search button.";

const char kUnifiedConsentName[] = "Unified Consent";
const char kUnifiedConsentDescription[] =
    "Enables a unified management of user consent for privacy-related "
    "features. This includes new confirmation screens and improved settings "
    "pages.";

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kWebPaymentsName[] = "Web Payments";
const char kWebPaymentsDescription[] =
    "Enable Payment Request API integration, a JavaScript API for merchants.";

const char kWebPaymentsNativeAppsName[] = "Web Payments Native Apps";
const char kWebPaymentsNativeAppsDescription[] =
    "Enable third party iOS native apps as payments methods within Payment "
    "Request.";

const char kWKHTTPSystemCookieStoreName[] = "Use WKHTTPSystemCookieStore.";
const char kWKHTTPSystemCookieStoreDescription[] =
    "Use WKHTTPCookieStore backed store for main context URL requests.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
