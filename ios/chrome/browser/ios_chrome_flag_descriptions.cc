// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDownstreamUseGooglePayBrandingOniOSName[] =
    "Enable Google Pay branding when offering credit card downstream";
const char kAutofillDownstreamUseGooglePayBrandingOniOSDescription[] =
    "When enabled, shows the Google Pay logo animation when showing payments"
    "credit card suggestions in downstream keyboard accessory";

const char kEnableAutofillCreditCardUploadUpdatePromptExplanationName[] =
    "Enable updated prompt explanation when offering credit card upload";
const char kEnableAutofillCreditCardUploadUpdatePromptExplanationDescription[] =
    "If enabled, changes the server save card prompt's explanation to mention "
    "the saving of the billing address.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncStandaloneTransportName[] = "Allow Sync standalone transport";
const char kSyncStandaloneTransportDescription[] =
    "If enabled, allows Chrome Sync to start in standalone transport mode. In "
    "this mode, the Sync machinery can start without user opt-in, but only a "
    "subset of data types are supported.";

const char kSyncSupportSecondaryAccountName[] =
    "Support secondary accounts for Sync standalone transport";
const char kSyncSupportSecondaryAccountDescription[] =
    "If enabled, allows Chrome Sync to start in standalone transport mode for "
    "a signed-in account that has not been chosen as Chrome's primary account. "
    "This only has an effect if sync-standalone-transport is also enabled.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kAppLauncherRefreshName[] = "Enable the new AppLauncher logic";
const char kAppLauncherRefreshDescription[] =
    "AppLauncher will always prompt if there is no direct link navigation, "
    "also Apps will launch asynchronously and there will be no logic that"
    "depends on the success or the failure of launching an app.";

const char kAutofillDynamicFormsName[] = "Autofill dynamic forms";
const char kAutofillDynamicFormsDescription[] =
    "Refills forms that dynamically change after an initial fill";

const char kAutofillPrefilledFieldsName[] = "Autofill prefilled forms";
const char kAutofillPrefilledFieldsDescription[] =
    "Fills forms that contain a programmatically filled value.";

const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[] =
    "Autofill Enforce Min Required Fields For Heuristics";
const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before allowing heuristic field-type prediction to occur.";

const char kAutofillEnforceMinRequiredFieldsForQueryName[] =
    "Autofill Enforce Min Required Fields For Query";
const char kAutofillEnforceMinRequiredFieldsForQueryDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before querying the autofill server for field-type predictions.";

const char kAutofillEnforceMinRequiredFieldsForUploadName[] =
    "Autofill Enforce Min Required Fields For Upload";
const char kAutofillEnforceMinRequiredFieldsForUploadDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fillable fields before uploading field-type votes for that form.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillManualFallbackName[] = "Enable autofill manual fallback";
const char kAutofillManualFallbackDescription[] =
    "When enabled, it shows the autofill UI with manual fallback when filling "
    "forms.";

const char kAutofillShowAllSuggestionsOnPrefilledFormsName[] =
    "Enable showing all suggestions when focusing prefilled field";
const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[] =
    "When enabled: show all suggestions when the focused field value has not "
    "been entered by the user. When disabled: use the field value as a filter.";

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillUpstreamUseGooglePayBrandingOnMobileName[] =
    "Enable Google Pay branding when offering credit card upload";
const char kAutofillUpstreamUseGooglePayBrandingOnMobileDescription[] =
    "If enabled, shows the Google Pay logo and a shorter header message when "
    "credit card upload to Google Payments is offered.";

const char kBrowserContainerFullscreenName[] = "Browser Container Fullscreen";
const char kBrowserContainerFullscreenDescription[] =
    "When enabled, the BrowserContainer is fullscreen. No UI change should be "
    "visible.";

const char kBrowserTaskScheduler[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

const char kCaptivePortalMetricsName[] = "Captive Portal Metrics";
const char kCaptivePortalMetricsDescription[] =
    "When enabled, some network issues will trigger a test to check if a "
    "Captive Portal network is the cause of the issue.";

const char kContextualSearch[] = "Contextual Search";
const char kContextualSearchDescription[] =
    "Whether or not Contextual Search is enabled.";

const char kCopyImageName[] = "Copy Image";
const char kCopyImageDescription[] =
    "Enable copying image to system pasteboard via context menu.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kNewClearBrowsingDataUIName[] = "Clear Browsing Data UI";
const char kNewClearBrowsingDataUIDescription[] =
    "Enable new Clear Browsing Data UI.";

const char kExternalSearchName[] = "External Search";
const char kExternalSearchDescription[] = "Enable support for External Search.";

const char kFullscreenViewportAdjustmentExperimentName[] =
    "Fullscreen Viewport Adjustment Mode";
const char kFullscreenViewportAdjustmentExperimentDescription[] =
    "The different ways in which the web view's viewport is updated for scroll "
    "events.  The default option updates the web view's frame.";

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

const char kITunesUrlsStoreKitHandlingName[] =
    "Store kit handling for ITunes links";
const char kITunesUrlsStoreKitHandlingDescription[] =
    "When enabled, opening itunes product URLs will be handled using the store "
    "kit.";

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

const char kNewPasswordFormParsingName[] = "New password form parsing";
const char kNewPasswordFormParsingDescription[] =
    "Replaces existing form parsing in password manager with a new version, "
    "currently under development. WARNING: when enabled Password Manager might "
    "stop working";

const char kOmniboxUIElideSuggestionUrlAfterHostName[] =
    "Hide the path, query, and ref of omnibox suggestions";
const char kOmniboxUIElideSuggestionUrlAfterHostDescription[] =
    "Elides the path, query, and ref of suggested URLs in the omnibox "
    "dropdown.";

const char kOutOfWebFullscreenName[] = "Fullscreen implementation out of web";
const char kOutOfWebFullscreenDescription[] =
    "Use the fullscreen implementation living outside of web. Disable the one "
    "in web.";

const char kPasswordExportName[] = "Password Export";
const char kPasswordExportDescription[] =
    "Enables password exporting functionality in password settings.";

const char kPhysicalWeb[] = "Physical Web";
const char kPhysicalWebDescription[] =
    "When enabled, the omnibox will include suggestions for web pages "
    "broadcast by devices near you.";

const char kInfobarsUIRebootName[] = "Infobars UI Reboot";
const char kInfobarsUIRebootDescription[] =
    "When enabled, Infobars will use the new UI Reboot layout.";

const char kSlimNavigationManagerName[] = "Use Slim Navigation Manager";
const char kSlimNavigationManagerDescription[] =
    "When enabled, uses the experimental slim navigation manager that provides "
    "better compatibility with HTML navigation spec.";

const char kWebErrorPagesName[] =
    "Present load errors in web contents area, instead of using "
    "CRWNativeContent";
const char kWebErrorPagesDescription[] =
    "When enabled the error pages will load in web contents area, instead of "
    "presenting CRWNativeContent. Does not have sensible UI changes.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kTabSwitcherPresentsBVCName[] = "TabSwitcher Presents BVC";
const char kTabSwitcherPresentsBVCDescription[] =
    "When enabled, the tab switcher will present the BVC, so that when the "
    "BVC is visible, the tab switcher will remain in the VC hierarchy "
    "underneath it.";

const char kUIRefreshLocationBarName[] = "UI Refresh Location Bar";
const char kUIRefreshLocationBarDescription[] =
    "When enabled, the UI Refresh location bar with a custom steady-state will "
    "be displayed.";

const char kUIRefreshPhase1Name[] = "UI Refresh Phase 1";
const char kUIRefreshPhase1Description[] =
    "When enabled, the first phase of the iOS UI refresh will be displayed.";

const char kUnifiedConsentName[] = "Unified Consent";
const char kUnifiedConsentDescription[] =
    "Enables a unified management of user consent for privacy-related "
    "features. This includes new confirmation screens and improved settings "
    "pages.";

const char kForceUnifiedConsentBumpName[] = "Force Unified Consent Bump";
const char kForceUnifiedConsentBumpDescription[] =
    "Force the unified consent bump UI to be shown on every start-up. This "
    "flag is for debug purpose, to test the UI.";

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kWebFrameMessagingName[] = "Web Frame Messaging";
const char kWebFrameMessagingDescription[] =
    "When enabled, API will be injected into webpages to allow sending messages"
    " directly to any frame of a webpage.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

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
