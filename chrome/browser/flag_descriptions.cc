// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

// Keep in identical order as the header file, see the comment at the top
// for formatting rules.

namespace flag_descriptions {

const char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
const char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

const char kAcceleratedVideoDecodeName[] = "Hardware-accelerated video decode";
const char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

const char kAffiliationBasedMatchingName[] =
    "Affiliation based matching in password manager";
const char kAffiliationBasedMatchingDescription[] =
    "Allow credentials stored for Android applications to be filled into "
    "corresponding websites.";

const char kAllowInsecureLocalhostName[] =
    "Allow invalid certificates for resources loaded from localhost.";
const char kAllowInsecureLocalhostDescription[] =
    "Allows requests to localhost over HTTPS even when an invalid certificate "
    "is presented.";

const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[] =
    "Allow Signed HTTP Exchange certificates without extension";
const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[] =
    "Accepts Origin-Signed HTTP Exchanges to be signed with certificates "
    "that do not have CanSignHttpExchangesDraft extension. Requires "
    "#enable-signed-http-exchange. Warning: Enabling this may pose a security "
    "risk.";

const char kAllowStartingServiceManagerOnlyName[] =
    "Allow starting service manager only";
const char kAllowStartingServiceManagerOnlyDescription[] =
    "Allows running a lightweight service-manager-only mode, in which services "
    "can run without the browser process.";

const char kUseMessagesGoogleComDomainName[] = "Use messages.google.com domain";
const char kUseMessagesGoogleComDomainDescription[] =
    "Use the messages.google.com domain as part of the \"Messages\" "
    "feature under \"Connected Devices\" settings.";

const char kUseMessagesStagingUrlName[] = "Use Messages staging URL";
const char kUseMessagesStagingUrlDescription[] =
    "Use the staging server as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kEnableMessagesWebPushName[] =
    "Web push in Android Messages integration";
const char kEnableMessagesWebPushDescription[] =
    "Use web push for background notificatons in Chrome OS integration "
    "with Android Messages for Web";

const char kAndroidSiteSettingsUIRefreshName[] =
    "Android Site Settings UI changes.";
const char kAndroidSiteSettingsUIRefreshDescription[] =
    "Enable the new UI "
    "changes in Site Settings in Android.";

const char kAppBannersName[] = "App Banners";
const char kAppBannersDescription[] =
    "Enable the display of Progressive Web App banners, which prompt a user to "
    "add a web app to their shelf, or other platform-specific equivalent.";

const char kAutomaticPasswordGenerationName[] = "Automatic password generation";
const char kAutomaticPasswordGenerationDescription[] =
    "Allow Chrome to offer to generate passwords when it detects account "
    "creation pages.";

const char kEnableBlinkHeapUnifiedGarbageCollectionName[] =
    "Blink Heap Unified Garbage Collection";
const char kEnableBlinkHeapUnifiedGarbageCollectionDescription[] =
    "Enable unified garbage collection in Blink";

const char kEnableBloatedRendererDetectionName[] = "Bloated renderer detection";
const char kEnableBloatedRendererDetectionDescription[] =
    "Enable bloated renderer detection";

const char kAsyncImageDecodingName[] = "AsyncImageDecoding";
const char kAsyncImageDecodingDescription[] =
    "Enables asynchronous decoding of images from raster for web content";

extern const char kAutofillAlwaysShowServerCardsInSyncTransportName[] =
    "AlwaysShowServerCardsInSyncTransport";
extern const char kAutofillAlwaysShowServerCardsInSyncTransportDescription[] =
    "Always show server cards when in sync transport mode for wallet data";

extern const char kAutofillAssistantChromeEntryName[] =
    "AutofillAssistantChromeEntry";
extern const char kAutofillAssistantChromeEntryDescription[] =
    "Initiate autofill assistant from within Chrome.";

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillDynamicFormsName[] = "Autofill Dynamic Forms";
const char kAutofillDynamicFormsDescription[] =
    "Allows autofill to fill dynamically changing forms";

const char kAutofillNoLocalSaveOnUploadSuccessName[] =
    "Disable saving local copy of uploaded card when credit card upload "
    "succeeds";
const char kAutofillNoLocalSaveOnUploadSuccessDescription[] =
    "When enabled, no local copy of server card will be saved when credit card "
    "upload succeeds.";

const char kAutofillPrefilledFieldsName[] = "Autofill Prefilled Fields";
const char kAutofillPrefilledFieldsDescription[] =
    "Allows autofill to fill fields previously filled by the website";

const char kAutofillProfileClientValidationName[] =
    "Autofill Validates Profiles By Client";
const char kAutofillProfileClientValidationDescription[] =
    "Allows autofill to validate profiles on the client side";

const char kAutofillProfileServerValidationName[] =
    "Autofill Uses Server Validation";
const char kAutofillProfileServerValidationDescription[] =
    "Allows autofill to use server side validation";

const char kAutofillShowFullDisclosureLabelName[] =
    "Autofill Show Full Disclosure Label";
const char kAutofillShowFullDisclosureLabelDescription[] =
    "When enabled, the Autofill dropdown's labels are displayed in the full "
    "disclosure format.";

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

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

const char kAutofillSettingsSplitByCardTypeName[] =
    "Autofill settings split by card type";
const char kAutofillSettingsSplitByCardTypeDescription[] =
    "When enabled, the cards in the payments settings will be split into two "
    "lists based on where they are stored.";

const char kAutoplayPolicyName[] = "Autoplay policy";
const char kAutoplayPolicyDescription[] =
    "Policy used when deciding if audio or video is allowed to autoplay.";

const char kAutoplayPolicyUserGestureRequiredForCrossOrigin[] =
    "User gesture is required for cross-origin iframes.";
const char kAutoplayPolicyNoUserGestureRequired[] =
    "No user gesture is required.";
const char kAutoplayPolicyUserGestureRequired[] = "User gesture is required.";
const char kAutoplayPolicyDocumentUserActivation[] =
    "Document user activation is required.";

const char kAwaitOptimizationName[] = "Await optimization";
const char kAwaitOptimizationDescription[] =
    "Enables await taking 1 tick on the microtask queue.";

const char kBleAdvertisingInExtensionsName[] = "BLE Advertising in Chrome Apps";
const char kBleAdvertisingInExtensionsDescription[] =
    "Enables BLE Advertising in Chrome Apps. BLE Advertising might interfere "
    "with regular use of Bluetooth Low Energy features.";

const char kBlockTabUndersName[] = "Block tab-unders";
const char kBlockTabUndersDescription[] =
    "Blocks tab-unders in Chrome with some native UI to allow the user to "
    "proceed.";

const char kBrowserTaskSchedulerName[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

const char kBundledConnectionHelpName[] = "Bundled Connection Help";
const char kBundledConnectionHelpDescription[] =
    "Enables or disables redirection to local help content for users who get "
    "an interstitial after clicking the 'Learn More' link on a previous "
    "interstitial.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kClickToOpenPDFName[] = "Click to open embedded PDFs";
const char kClickToOpenPDFDescription[] =
    "When the PDF plugin is unavailable, show a click-to-open placeholder for "
    "embedded PDFs.";

const char kClipboardContentSettingName[] = "Clipboard content setting";
const char kClipboardContentSettingDescription[] =
    "Enables a site-wide permission in the UI which controls access to the "
    "asynchronous clipboard web API";

const char kCloudImportName[] = "Cloud Import";
const char kCloudImportDescription[] = "Allows the cloud-import feature.";

const char kCloudPrinterHandlerName[] = "Enable Cloud Printer Handler";
const char kCloudPrinterHandlerDescription[] =
    "Use the new cloud printer handler for communicating with the cloud "
    "print server, instead of the cloud print interface in the Print "
    "Preview WebUI.";

const char kFCMInvalidationsName[] =
    "Enable invalidations delivery via new FCM based protocol";
const char kFCMInvalidationsDescription[] =
    "Use the new FCM-based protocol for deliveling invalidations";

const char kFocusMode[] = "Focus Mode";
const char kFocusModeDescription[] =
    "If enabled, allows the user to switch to Focus Mode";

const char kForceColorProfileSRGB[] = "sRGB";
const char kForceColorProfileP3[] = "Display P3 D65";
const char kForceColorProfileColorSpin[] = "Color spin with gamma 2.4";
const char kForceColorProfileHdr[] = "scRGB linear (HDR where available)";

const char kForceColorProfileName[] = "Force color profile";
const char kForceColorProfileDescription[] =
    "Forces Chrome to use a specific color profile instead of the color "
    "of the window's current monitor, as specified by the operating system.";

const char kCompositedLayerBordersName[] = "Composited render layer borders";
const char kCompositedLayerBordersDescription[] =
    "Renders a border around composited Render Layers to help debug and study "
    "layer compositing.";

const char kContextualSuggestionsButtonName[] = "Contextual Suggestions Button";
const char kContextualSuggestionsButtonDescription[] =
    "If enabled, shows a button to trigger contextual suggestions.";

const char kContextualSuggestionsIPHReverseScrollName[] =
    "Contextual Suggestions IPH Reverse Scroll";
const char kContextualSuggestionsIPHReverseScrollDescription[] =
    "Require a reverse scroll before showing in-product help for contextual "
    "suggestions.";

const char kContextualSuggestionsOptOutName[] =
    "Contextual Suggestions Opt-out";
const char kContextualSuggestionsOptOutDescription[] =
    "If enabled, allows the user to opt out of contextual suggestions.";

const char kCreditCardAssistName[] = "Credit Card Assisted Filling";
const char kCreditCardAssistDescription[] =
    "Enable assisted credit card filling on certain sites.";

const char kDataSaverServerPreviewsName[] = "Data Saver Server Previews";
const char kDataSaverServerPreviewsDescription[] =
    "Allow the Data Reduction Proxy to serve previews.";

const char kDebugPackedAppName[] = "Debugging for packed apps";
const char kDebugPackedAppDescription[] =
    "Enables debugging context menu options such as Inspect Element for packed "
    "applications.";

const char kDebugShortcutsName[] = "Debugging keyboard shortcuts";
const char kDebugShortcutsDescription[] =
    "Enables additional keyboard shortcuts that are useful for debugging Ash.";

const char kDeviceDiscoveryNotificationsName[] =
    "Device Discovery Notifications";
const char kDeviceDiscoveryNotificationsDescription[] =
    "Device discovery notifications on local network.";

const char kDevtoolsExperimentsName[] = "Developer Tools experiments";
const char kDevtoolsExperimentsDescription[] =
    "Enables Developer Tools experiments. Use Settings panel in Developer "
    "Tools to toggle individual experiments.";

const char kDisableAudioForDesktopShareName[] =
    "Disable Audio For Desktop Share";
const char kDisableAudioForDesktopShareDescription[] =
    "With this flag on, desktop share picker window will not let the user "
    "choose whether to share audio.";

const char kDisableBestEffortTasksName[] = "Skip best effort tasks";
const char kDisableBestEffortTasksDescription[] =
    "With this flag on, tasks of the lowest priority will not be executed "
    "until shutdown. The queue of low priority tasks can increase memory usage."
    "Also, while it should be possible to use Chrome almost normally with this "
    "flag, it is expected that some non-visible operations such as writing "
    "user data to disk, cleaning caches, reporting metrics or updating "
    "components won't be performed until shutdown.";

const char kDisableIpcFloodingProtectionName[] =
    "Disable IPC flooding protection";
const char kDisableIpcFloodingProtectionDescription[] =
    "Some javascript code can flood the inter process communication system. "
    "This protection limits the rate (calls/seconds) at which theses function "
    "can be used. This flag disables the protection. This flag is deprecated "
    "and will be removed in Chrome 76. Use the switch "
    "--disable-ipc-flooding-protection instead.";

const char kDisablePushStateThrottleName[] = "Disable pushState throttling";
const char kDisablePushStateThrottleDescription[] =
    "Disables throttling of history.pushState and history.replaceState method "
    "calls. This flag is deprecated and will be removed in Chrome 76. Use the "
    "switch --disable-ipc-flooding-protection instead.";

const char kDisableTabForDesktopShareName[] =
    "Disable Desktop Share with tab source";
const char kDisableTabForDesktopShareDescription[] =
    "This flag controls whether users can choose a tab for desktop share.";

const char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
const char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

const char kDisallowUnsafeHttpDownloadsName[] =
    "Block unsafe downloads over insecure connections";
const char kDisallowUnsafeHttpDownloadsNameDescription[] =
    "Disallows downloads of unsafe files (files that can potentially execute "
    "code), where the final download origin or any origin in the redirect "
    "chain is insecure.";

const char kEmbeddedExtensionOptionsName[] = "Embedded extension options";
const char kEmbeddedExtensionOptionsDescription[] =
    "Display extension options as an embedded element in chrome://extensions "
    "rather than opening a new tab.";

const char kEnableAccessibilityObjectModelName[] = "Accessibility Object Model";
const char kEnableAccessibilityObjectModelDescription[] =
    "Enables experimental support for Accessibility Object Model APIs "
    "that are in development.";

const char kEnableAudioFocusEnforcementName[] = "Audio Focus Enforcement";
const char kEnableAudioFocusEnforcementDescription[] =
    "Enables enforcement of a single media session having audio focus at "
    "any one time. Requires #enable-media-session-service to be enabled too.";

const char kEnableAutocompleteDataRetentionPolicyName[] =
    "Enable automatic cleanup of expired Autocomplete entries.";
const char kEnableAutocompleteDataRetentionPolicyDescription[] =
    "If enabled, will clean-up Autocomplete entries whose last use date is "
    "older than the current retention policy. These entries will be "
    "permanently deleted from the client on startup, and will be unlinked "
    "from sync.";

const char kEnableAutofillAccountWalletStorageName[] =
    "Enable the account data storage for autofill";
const char kEnableAutofillAccountWalletStorageDescription[] =
    "Enable the ephemeral storage for account data for autofill.";

const char kEnableAutofillCreditCardAblationExperimentDisplayName[] =
    "Credit card autofill ablation experiment.";
const char kEnableAutofillCreditCardAblationExperimentDescription[] =
    "If enabled, credit card autofill suggestions will not display.";

const char kEnableAutofillCreditCardLastUsedDateDisplayName[] =
    "Display the last used date of a credit card in autofill.";
const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[] =
    "If enabled, display the last used date of a credit card in autofill.";

const char kEnableAutofillCreditCardLocalCardMigrationName[] =
    "Enable migrating local cards to Google Pay";
const char kEnableAutofillCreditCardLocalCardMigrationDescription[] =
    "If enabled, prompt migration of locally-saved credit cards to Google Pay.";

const char kEnableAutofillCreditCardUploadEditableCardholderNameName[] =
    "Make cardholder name editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableCardholderNameDescription[] =
    "If enabled, in certain situations when offering credit card upload to "
    "Google Payments, the cardholder name can be edited within the "
    "offer-to-save dialog, which is prefilled with the name from the signed-in "
    "Google Account.";

const char kEnableAutofillCreditCardUploadEditableExpirationDateName[] =
    "Make expiration date editable in dialog during credit card upload";
const char kEnableAutofillCreditCardUploadEditableExpirationDateDescription[] =
    "If enabled, if a credit card's expiration date was not detected when "
    "offering card upload to Google Payments, the offer-to-save dialog "
    "displays an expiration date selector.";

const char kEnableAutofillDoNotUploadSaveUnsupportedCardsName[] =
    "Prevents upload save on cards from unsupported networks";
const char kEnableAutofillDoNotUploadSaveUnsupportedCardsDescription[] =
    "If enabled, cards from unsupported networks will not be offered upload "
    "save, and will instead be offered local save.";

const char kEnableAutofillImportNonFocusableCreditCardFormsName[] =
    "Allow credit card import from forms that disappear after entry";
const char kEnableAutofillImportNonFocusableCreditCardFormsDescription[] =
    "If enabled, offers credit card save for forms that are hidden from the "
    "page after information has been entered into them, including "
    "accordion-style checkout flows.";

const char kEnableAutofillLocalCardMigrationShowFeedbackName[] =
    "Show the upload results dialog after local card migration";
const char kEnableAutofillLocalCardMigrationShowFeedbackDescription[] =
    "If enabled, the local card migration offer dialog will remain pending "
    "after the user clicks the save button. Once migration is finished, "
    "the dialog will be updated with the results of each card.";

const char kEnableAutofillLocalCardMigrationUsesStrikeSystemV2Name[] =
    "Enable limit on offering to migrate local cards repeatedly using the "
    "updated strike system implementation";
const char kEnableAutofillLocalCardMigrationUsesStrikeSystemV2Description[] =
    "If enabled, uses the updated strike system implementation to prevent "
    "offering prompts for local card migration if it has repeatedly been "
    "ignored, declined, or failed.";

const char kEnableAutofillNativeDropdownViewsName[] =
    "Display Autofill Dropdown Using Views";
const char kEnableAutofillNativeDropdownViewsDescription[] =
    "If enabled, the Autofill Dropdown will be built natively using Views, "
    "rather than painted directly to a canvas.";

const char kEnableAutofillSaveCardDialogUnlabeledExpirationDateName[] =
    "Show unlabeled expiration dates on the save card dialog";
const char kEnableAutofillSaveCardDialogUnlabeledExpirationDateDescription[] =
    "If enabled, expiration dates on the save card dialog (both local and "
    "upstream) are shown without an 'Exp:' label.";

const char kEnableAutofillSaveCardImprovedUserConsentName[] =
    "Use updated UI for credit card save bubbles";
const char kEnableAutofillSaveCardImprovedUserConsentDescription[] =
    "If enabled, adds a [No thanks] button to credit card save bubbles and "
    "updates their title headers.";

const char kEnableAutofillSaveCardSignInAfterLocalSaveName[] =
    "Show Sign-In/Sync promo after saving a card locally";
const char kEnableAutofillSaveCardSignInAfterLocalSaveDescription[] =
    "If enabled, shows a sign in prompt to the user after the user "
    "saves a card locally. This also introduces a Manage Cards bubble "
    "which you can access from the card icon after saving a card.";

const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[] =
    "Enable limit on offering to save the same credit card repeatedly";
const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[] =
    "If enabled, prevents popping up the credit card offer-to-save prompt if "
    "it has repeatedly been ignored, declined, or failed.";

const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Name[] =
    "Enable limit on offering to save the same credit card repeatedly using "
    "the updated strike system implementation";
const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Description[] =
    "If enabled, uses the updated strike system implementation to prevent "
    "popping up the credit card offer-to-save prompt if it has repeatedly been "
    "ignored, declined, or failed.";

const char kEnableAutofillSendExperimentIdsInPaymentsRPCsName[] =
    "Send experiment flag IDs in calls to Google Payments";
const char kEnableAutofillSendExperimentIdsInPaymentsRPCsDescription[] =
    "If enabled, adds the status of certain experiment variations when making "
    "calls to Google Payments.";

const char kEnableAutoplayIgnoreWebAudioName[] =
    "Autoplay ignores WebAudio playbacks";
const char kEnableAutoplayIgnoreWebAudioDescription[] =
    "If enabled, autoplay restrictions will be ignored for WebAudio.";

const char kEnableAutoplayUnifiedSoundSettingsName[] =
    "Autoplay unified sound settings UI";
const char kEnableAutoplayUnifiedSoundSettingsDescription[] =
    "If enabled, shows the new unified autoplay sound settings UI.";

const char kEnableBrotliName[] = "Brotli Content-Encoding.";
const char kEnableBrotliDescription[] =
    "Enable Brotli Content-Encoding support.";

const char kEnableDataSaverLiteModeRebrandName[] =
    "Data Saver Lite Mode Rebranding";
const char kEnableDataSaverLiteModeRebrandDescription[] =
    "Enable the Data Saver rebranding to Lite Mode.";

const char kEnableClientLoFiName[] = "Client-side Lo-Fi previews";

const char kEnableClientLoFiDescription[] =
    "Enable showing low fidelity images on some pages on slow networks.";

const char kEnableCSSFragmentIdentifiersName[] =
    "Enable CSS Fragment Identifiers";
const char kEnableCSSFragmentIdentifiersDescription[] =
    "Enable support for specifying a target element using a css selector in "
    "the fragment identifier.";

const char kEnableFilesystemInIncognitoName[] = "Filesystem API in Incognito";
const char kEnableFilesystemInIncognitoDescription[] =
    "Enable Filesystem API in incognito mode.";

const char kEnableNoScriptPreviewsName[] = "NoScript previews";

const char kEnableNoScriptPreviewsDescription[] =
    "Enable disabling JavaScript on some pages on slow networks.";

const char kDataReductionProxyServerAlternative1[] = "Use alt. server config 1";
const char kDataReductionProxyServerAlternative2[] = "Use alt. server config 2";
const char kDataReductionProxyServerAlternative3[] = "Use alt. server config 3";
const char kDataReductionProxyServerAlternative4[] = "Use alt. server config 4";
const char kDataReductionProxyServerAlternative5[] = "Use alt. server config 5";
const char kDataReductionProxyServerAlternative6[] = "Use alt. server config 6";
const char kDataReductionProxyServerAlternative7[] = "Use alt. server config 7";
const char kDataReductionProxyServerAlternative8[] = "Use alt. server config 8";
const char kDataReductionProxyServerAlternative9[] = "Use alt. server config 9";
const char kDataReductionProxyServerAlternative10[] =
    "Use alt. server config 10";
const char kEnableDataReductionProxyNetworkServiceName[] =
    "Data reduction proxy with network service";
const char kEnableDataReductionProxyNetworkServiceDescription[] =
    "Enable data reduction proxy when network service is enabled";
const char kEnableDataReductionProxyServerExperimentName[] =
    "Use an alternative Data Saver back end configuration.";
const char kEnableDataReductionProxyServerExperimentDescription[] =
    "Enable a different approach to saving data by configuring the back end "
    "server";

const char kEnableDataReductionProxySavingsPromoName[] =
    "Data Saver 1 MB Savings Promo";
const char kEnableDataReductionProxySavingsPromoDescription[] =
    "Enable a Data Saver promo for 1 MB of savings. If Data Saver has already "
    "saved 1 MB of data, then the promo will not be shown. Data Saver must be "
    "enabled for the promo to be shown.";

const char kEnableDesktopPWAsName[] = "Desktop PWAs";
const char kEnableDesktopPWAsDescription[] =
    "Experimental windowing and install banner treatment for Progressive Web "
    "Apps on desktop platforms. Implies #enable-experimental-app-banners.";

const char kEnableDesktopPWAsLinkCapturingName[] =
    "Desktop PWAs Link Capturing";
const char kEnableDesktopPWAsLinkCapturingDescription[] =
    "Experimentally enable link capturing for Desktop PWAs. Navigations to "
    "URLs that are in-scope of Desktop PWAs will open in a window. Requires "
    "#enable-desktop-pwas.";

const char kDesktopPWAsCustomTabUIName[] = "Desktop PWAs Custom Tab UI";
const char kDesktopPWAsCustomTabUIDescription[] =
    "Browsing out-of-scope links in a desktop PWA will use the custom tab UI "
    "for displaying the page title and origin instead of the location bar.";

const char kDesktopPWAsStayInWindowName[] =
    "Desktop PWAs out-of-scope links open in the app window";
const char kDesktopPWAsStayInWindowDescription[] =
    "Links to sites in a different scope will open inside the PWA window as "
    "opposed to in the browser.";

const char kDesktopPWAsOmniboxInstallName[] =
    "Desktop PWAs installable from the omnibox";
const char kDesktopPWAsOmniboxInstallDescription[] =
    "When on a site that passes PWA installation requirements show a button in "
    "the omnibox for installing it.";

const char kEnableSystemWebAppsName[] = "System Web Apps";
const char kEnableSystemWebAppsDescription[] =
    "Experimental system for using the Desktop PWA framework for running System"
    "Apps (e.g Settings, Discover).";

const char kEnforceTLS13DowngradeName[] = "TLS 1.3 downgrade hardening";
const char kEnforceTLS13DowngradeDescription[] =
    "This option enables the TLS 1.3 downgrade hardening mechanism. This "
    "hardens TLS 1.3 connections while remaining compatible with TLS 1.0 "
    "through 1.2 connections. Firewalls and proxies that do not function when "
    "this is enabled do not implement TLS 1.0 through 1.2 correctly or "
    "securely. They must be fixed by vendors.";

const char kEnableGenericSensorName[] = "Generic Sensor";
const char kEnableGenericSensorDescription[] =
    "Enables motion sensor classes based on Generic Sensor API, i.e. "
    "Accelerometer, LinearAccelerationSensor, Gyroscope, "
    "AbsoluteOrientationSensor and RelativeOrientationSensor interfaces.";

const char kEnableGenericSensorExtraClassesName[] =
    "Generic Sensor Extra Classes";
const char kEnableGenericSensorExtraClassesDescription[] =
    "Enables an extra set of sensor classes based on Generic Sensor API, which "
    "expose previously unavailable platform features, i.e. AmbientLightSensor "
    "and Magnetometer interfaces.";

const char kEnableGpuServiceLoggingName[] = "Enable gpu service logging";
const char kEnableGpuServiceLoggingDescription[] =
    "Enable printing the actual GL driver calls.";

const char kEnableHDRName[] = "HDR mode";
const char kEnableHDRDescription[] =
    "Enables HDR support on compatible displays.";

const char kEnableImplicitRootScrollerName[] = "Implicit Root Scroller";
const char kEnableImplicitRootScrollerDescription[] =
    "Enables implicitly choosing which scroller on a page is the 'root "
    "scroller'. i.e. The one that gets special features like URL bar movement, "
    "overscroll glow, rotation anchoring, etc.";

const char kEnablePreviewsAndroidOmniboxUIName[] =
    "Previews Android Omnibox UI";
const char kEnablePreviewsAndroidOmniboxUIDescription[] =
    "Enable showing the Previews UI in the Omnibox on Android instead of an "
    "InfoBar. This has no effect on other platforms.";

const char kEnableLitePageServerPreviewsName[] = "Lite Page Server Previews";
const char kEnableLitePageServerPreviewsDescription[] =
    "Enable showing Lite Page Previews served from a Previews Server."
    "This feature will cause Chrome to redirect eligible navigations "
    "to a Google-owned domain that serves a pre-rendered version of the "
    "original page. Also known as Lite Page Redirect Previews.";

const char kEnableURLLoaderLitePageServerPreviewsName[] =
    "Lite Page Server Previews using URL Loader";
const char kEnableURLLoaderLitePageServerPreviewsDescription[] =
    "Enable using a network service URL Loader for Lite Page Server Previews. "
    "This requires enable-lite-page-server-previews to be enabled along with "
    "network-service.";

const char kBuiltInModuleAllName[] = "All experimental built-in modules";
const char kBuiltInModuleAllDescription[] =
    "Enable all experimental built-in modules, as well as built-in module "
    "infrastructure and import maps. The syntax and the APIs exposed are "
    "experimental and will change over time.";

const char kBuiltInModuleInfraName[] = "Built-in module infra and import maps";
const char kBuiltInModuleInfraDescription[] =
    "Enable built-in module infrastructure and import maps. Individual "
    "built-in modules should be enabled by other flags. The syntax and the "
    "APIs exposed are experimental and will change over time.";

const char kBuiltInModuleKvStorageName[] = "kv-storage built-in module";
const char kBuiltInModuleKvStorageDescription[] =
    "Enable kv-storage built-in module, as well as built-in module "
    "infrastructure and import maps. The syntax and the APIs exposed are "
    "experimental and will change over time.";

const char kEnableBlinkGenPropertyTreesName[] = "Enable BlinkGenPropertyTrees";
const char kEnableBlinkGenPropertyTreesDescription[] =
    "Enable a new compositing mode where Blink generates the compositor "
    "property trees.";

const char kEnableLayoutNGName[] = "Enable LayoutNG";
const char kEnableLayoutNGDescription[] =
    "Enable Blink's next generation layout engine.";

const char kEnableLazyFrameLoadingName[] = "Enable lazy frame loading";
const char kEnableLazyFrameLoadingDescription[] =
    "Defers the loading of certain cross-origin frames until the page is "
    "scrolled down near them.";

const char kEnableLazyImageLoadingName[] = "Enable lazy image loading";
const char kEnableLazyImageLoadingDescription[] =
    "Defers the loading of images until the page is scrolled down near them.";

const char kEnableMacMaterialDesignDownloadShelfName[] =
    "Enable Material Design download shelf";

const char kEnableMacMaterialDesignDownloadShelfDescription[] =
    "If enabled, the download shelf uses Material Design.";

const char kEnableMediaSessionServiceName[] = "Media Session Service";
const char kEnableMediaSessionServiceDescription[] =
    "Enables the media session mojo service and internal media session "
    "support.";

const char kEnableNavigationTracingName[] = "Enable navigation tracing";
const char kEnableNavigationTracingDescription[] =
    "This is to be used in conjunction with the trace-upload-url flag. "
    "WARNING: When enabled, Chrome will record performance data for every "
    "navigation and upload it to the URL specified by the trace-upload-url "
    "flag. The trace may include personally identifiable information (PII) "
    "such as the titles and URLs of websites you visit.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kEnableNetworkServiceName[] = "Enable network service";
const char kEnableNetworkServiceDescription[] =
    "Enables the network service, which makes network requests through a "
    "separate process.";

const char kEnableNetworkServiceInProcessName[] =
    "Runs network service in-process";
const char kEnableNetworkServiceInProcessDescription[] =
    "Runs the network service in the browser process.";

const char kEnableNotificationScrollBarName[] =
    "Enable notification list scroll bar";
const char kEnableNotificationScrollBarDescription[] =
    "Enable the scroll bar of the notification list in Unified System Tray.";

const char kEnableNotificationExpansionAnimationName[] =
    "Enable notification expansion animations";
const char kEnableNotificationExpansionAnimationDescription[] =
    "Enable notification animations whenever the expansion state is toggled.";

const char kEnableOptimizationHintsName[] = "Optimization Hints";
const char kEnableOptimizationHintsDescription[] =
    "Enable the Optimization Hints feature which incorporates server hints "
    "into decisions for what optimizations to perform on some pages on slow "
    "networks.";

const char kEnableOutOfBlinkCorsName[] = "Out of blink CORS";
const char kEnableOutOfBlinkCorsDescription[] =
    "CORS handling logic is moved out of blink.";

const char kExperimentalAccessibilityFeaturesName[] =
    "Experimental accessibility features";
const char kExperimentalAccessibilityFeaturesDescription[] =
    "Enable additional accessibility features in the Settings page.";

const char kExperimentalAccessibilityAutoclickName[] =
    "Experimental automatic click features";
const char kExperimentalAccessibilityAutoclickDescription[] =
    "Enable additional features for automatic clicks.";

const char kExperimentalAccessibilityLanguageDetectionName[] =
    "Experimental accessibility language detection";
const char kExperimentalAccessibilityLanguageDetectionDescription[] =
    "Enable language detection for in-page content which is then exposed to "
    "accessiblity technologies such as screen readers.";

const char kExperimentalAccessibilitySwitchAccessName[] =
    "Experimental feature Switch Access";
const char kExperimentalAccessibilitySwitchAccessDescription[] =
    "Add a setting to enable the prototype of Switch Access";

const char kVizDisplayCompositorName[] = "Viz Display Compositor (OOP-D)";
const char kVizDisplayCompositorDescription[] =
    "If enabled, the display compositor runs as part of the viz service in the"
    "GPU process.";

const char kVizHitTestDrawQuadName[] = "Viz Hit-test Draw-quad version";
const char kVizHitTestDrawQuadDescription[] =
    "If enabled, event targeting uses the new viz-assisted hit-testing logic, "
    "with hit-test data computed from the CompositorFrame.";

const char kEnableOutOfProcessHeapProfilingName[] =
    "Chrome heap profiling start mode.";
const char kEnableOutOfProcessHeapProfilingDescription[] =
    "Starts heap profiling service that records sampled memory allocation "
    "profile having each sample attributed with a callstack. "
    "The sampling resolution is controlled with --memlog-sampling flag. "
    "Recorded heap dumps can be obtained at chrome://tracing "
    "[category:memory-infra] and chrome://memory-internals. This setting "
    "controls which processes are profiled. As long as this setting is not "
    "disabled, users can start profiling any given process in "
    "chrome://memory-internals.";
const char kEnableOutOfProcessHeapProfilingModeMinimal[] = "Browser and GPU";
const char kEnableOutOfProcessHeapProfilingModeAll[] = "All processes";
const char kEnableOutOfProcessHeapProfilingModeAllRenderers[] = "All renderers";
const char kEnableOutOfProcessHeapProfilingModeBrowser[] = "Only browser";
const char kEnableOutOfProcessHeapProfilingModeGpu[] = "Only GPU.";
const char kEnableOutOfProcessHeapProfilingModeManual[] =
    "None by default. Visit chrome://memory-internals to choose which "
    "processes to profile.";
const char kEnableOutOfProcessHeapProfilingModeRendererSampling[] =
    "Profile a random sampling of renderer processes, ensuring only one is "
    "ever profiled at a time.";

const char kOutOfProcessHeapProfilingInProcess[] =
    "Run the heap profiling service in the browser process.";
const char kOutOfProcessHeapProfilingInProcessDescription[] =
    "Makes profiling service (if enabled) to be executed within the browser "
    "process. By default the service is run in a dedicated utility process.";

const char kOutOfProcessHeapProfilingSamplingRate[] =
    "Sampling interval in bytes for memlog allocations.";
const char kOutOfProcessHeapProfilingSamplingRateDescription[] =
    "Use a poisson process to sample allocations. Defaults to a sampling rate "
    "of 100KB. This results in low noise for large and/or frequent allocations "
    "[size * frequency >> 100KB]. This means that aggregate numbers [e.g. "
    "total size of malloc-ed objects] and large and/or frequent allocations "
    "can be trusted with high fidelity.";

const char kOOPHPStackModeName[] =
    "The type of stack to record for memlog heap dumps";
const char kOOPHPStackModeDescription[] =
    "By default, memlog heap dumps record native stacks, which requires a "
    "post-processing step to symbolize. Requires a custom build with frame "
    "pointers to work on Android. Native with thread names will add the thread "
    "name as the first frame of each native stack. It's also possible to "
    "record a pseudo stack using trace events as identifiers. It's also "
    "possible to do a mix of both.";
const char kOOPHPStackModeMixed[] = "Mixed";
const char kOOPHPStackModeNative[] = "Native";
const char kOOPHPStackModeNativeWithThreadNames[] = "Native with thread names";
const char kOOPHPStackModePseudo[] = "Trace events";

const char kEnablePictureInPictureName[] = "Enable Picture-in-Picture.";
const char kEnablePictureInPictureDescription[] =
    "Show Picture-in-Picture in browser context menu and video native "
    "controls. The #enable-surfaces-for-videos flag must be enabled as well "
    "to use it.";

const char kEnablePixelCanvasRecordingName[] = "Enable pixel canvas recording";
const char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

const char kEnableResamplingInputEventsName[] =
    "Enable resampling input events";
const char kEnableResamplingInputEventsDescription[] =
    "Predicts mouse and touch inputs position at rAF time based on previous "
    "input";
const char kEnableResamplingScrollEventsName[] =
    "Enable resampling scroll events";
const char kEnableResamplingScrollEventsDescription[] =
    "Predicts the scroll amount at vsync time based on previous input";

const char kEnableResourceLoadingHintsName[] = "Enable resource loading hints";
const char kEnableResourceLoadingHintsDescription[] =
    "Enable using server-provided resource loading hints to provide a preview "
    "over slow network connections.";

const char kEnableSyncPseudoUSSAppListName[] =
    "Enable pseudo-USS for APP_LIST sync.";
const char kEnableSyncPseudoUSSAppListDescription[] =
    "Enable new USS-based codepath for sync datatype APP_LIST.";

const char kEnableSyncPseudoUSSAppsName[] = "Enable pseudo-USS for APPS sync.";
const char kEnableSyncPseudoUSSAppsDescription[] =
    "Enable new USS-based codepath for sync datatype APPS.";

const char kEnableSyncPseudoUSSDictionaryName[] =
    "Enable pseudo-USS for DICTIONARY sync.";
const char kEnableSyncPseudoUSSDictionaryDescription[] =
    "Enable new USS-based codepath for sync datatype DICTIONARY.";

const char kEnableSyncPseudoUSSExtensionSettingsName[] =
    "Enable pseudo-USS for EXTENSION_SETTINGS and APP_SETTINGS sync.";
const char kEnableSyncPseudoUSSExtensionSettingsDescription[] =
    "Enable new USS-based codepath for sync datatypes EXTENSION_SETTINGS and "
    "APP_SETTINGS.";

const char kEnableSyncPseudoUSSExtensionsName[] =
    "Enable pseudo-USS for EXTENSIONS sync.";
const char kEnableSyncPseudoUSSExtensionsDescription[] =
    "Enable new USS-based codepath for sync datatype EXTENSIONS.";

const char kEnableSyncPseudoUSSFaviconsName[] =
    "Enable pseudo-USS for favicon sync.";
const char kEnableSyncPseudoUSSFaviconsDescription[] =
    "Enable new USS-based codepath for sync datatypes FAVICON_IMAGES and "
    "FAVICON_TRACKING.";

const char kEnableSyncPseudoUSSHistoryDeleteDirectivesName[] =
    "Enable pseudo-USS for HISTORY_DELETE_DIRECTIVES sync.";
const char kEnableSyncPseudoUSSHistoryDeleteDirectivesDescription[] =
    "Enable new USS-based codepath for sync datatype "
    "HISTORY_DELETE_DIRECTIVES.";

const char kEnableSyncPseudoUSSPasswordsName[] =
    "Enable pseudo-USS for PASSWORDS sync.";
const char kEnableSyncPseudoUSSPasswordsDescription[] =
    "Enable new USS-based codepath for sync datatype PASSWORDS (pseudo-USS).";

const char kEnableSyncPseudoUSSPreferencesName[] =
    "Enable pseudo-USS for PREFERENCES sync.";
const char kEnableSyncPseudoUSSPreferencesDescription[] =
    "Enable new USS-based codepath for sync datatype PREFERENCES.";

const char kEnableSyncPseudoUSSPriorityPreferencesName[] =
    "Enable pseudo-USS for PRIORITY_PREFERENCES sync.";
const char kEnableSyncPseudoUSSPriorityPreferencesDescription[] =
    "Enable new USS-based codepath for sync datatype PRIORITY_PREFERENCES.";

const char kEnableSyncPseudoUSSSearchEnginesName[] =
    "Enable pseudo-USS for SEARCH_ENGINES sync.";
const char kEnableSyncPseudoUSSSearchEnginesDescription[] =
    "Enable new USS-based codepath for sync datatype SEARCH_ENGINES.";

const char kEnableSyncPseudoUSSSupervisedUsersName[] =
    "Enable pseudo-USS for supervised users sync.";
const char kEnableSyncPseudoUSSSupervisedUsersDescription[] =
    "Enable new USS-based codepath for sync datatypes SUPERVISED_USER_SETTINGS "
    "and SUPERVISED_USER_WHITELISTS.";

const char kEnableSyncPseudoUSSThemesName[] =
    "Enable pseudo-USS for THEMES sync.";
const char kEnableSyncPseudoUSSThemesDescription[] =
    "Enable new USS-based codepath for sync datatype THEMES.";

const char kEnableSyncUSSBookmarksName[] = "Enable USS for bookmarks sync";
const char kEnableSyncUSSBookmarksDescription[] =
    "Enables the new, experimental implementation of bookmark sync";

const char kEnableSyncUSSPasswordsName[] = "Enable USS for passwords sync";
const char kEnableSyncUSSPasswordsDescription[] =
    "Enables the new, experimental implementation of passwords sync";

const char kEnableSyncUSSSessionsName[] = "Enable USS for sessions sync";
const char kEnableSyncUSSSessionsDescription[] =
    "Enables the new, experimental implementation of session sync (aka tab "
    "sync).";

const char kEnableTextFragmentAnchorName[] = "Enable Text Fragment Anchor.";
const char kEnableTextFragmentAnchorDescription[] =
    "Enables scrolling to text specified in URL's fragment.";

const char kEnableUseZoomForDsfName[] =
    "Use Blink's zoom for device scale factor.";
const char kEnableUseZoomForDsfDescription[] =
    "If enabled, Blink uses its zooming mechanism to scale content for device "
    "scale factor.";
const char kEnableUseZoomForDsfChoiceDefault[] = "Default";
const char kEnableUseZoomForDsfChoiceEnabled[] = "Enabled";
const char kEnableUseZoomForDsfChoiceDisabled[] = "Disabled";

const char kEnableScrollAnchorSerializationName[] =
    "Scroll Anchor Serialization";
const char kEnableScrollAnchorSerializationDescription[] =
    "Save the scroll anchor and use it to restore the scroll position when "
    "navigating.";

const char kEnableSharedArrayBufferName[] =
    "Experimental enabled SharedArrayBuffer support in JavaScript.";
const char kEnableSharedArrayBufferDescription[] =
    "Enable SharedArrayBuffer support in JavaScript.";

const char kEnableWasmName[] = "WebAssembly structured cloning support.";
const char kEnableWasmDescription[] =
    "Enable web pages to use WebAssembly structured cloning.";

const char kEnableWebAuthenticationCableSupportName[] =
    "Web Authentication caBLE support";
const char kEnableWebAuthenticationCableSupportDescription[] =
    "Enable the cloud-assisted pairingless BLE protocol for use with "
    "the Web Authentication API.";

const char kEnableIncognitoWindowCounterName[] = "Incognito Window Counter";
const char kEnableIncognitoWindowCounterDescription[] =
    "Shows the count of Incognito windows next to the Incognito icon on the "
    "toolbar.";

const char kEnableWasmBaselineName[] = "WebAssembly baseline compiler";
const char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

const char kEnableWasmCodeCacheName[] = "WebAssembly compiled module cache";
const char kEnableWasmCodeCacheDescription[] =
    "Enables caching of compiled WebAssembly modules.";

const char kEnableWasmThreadsName[] = "WebAssembly threads support.";
const char kEnableWasmThreadsDescription[] =
    "Enables support for the WebAssembly Threads proposal. Implies "
    "#shared-array-buffer and #enable-webassembly.";

const char kExpensiveBackgroundTimerThrottlingName[] =
    "Throttle expensive background timers";
const char kExpensiveBackgroundTimerThrottlingDescription[] =
    "Enables intervention to limit CPU usage of background timers to 1%.";

const char kExperimentalAppBannersName[] = "Experimental app banners";
const char kExperimentalAppBannersDescription[] =
    "Enables a new experimental app banner flow and UI. Implies "
    "#enable-app-banners.";

const char kExperimentalCanvasFeaturesName[] = "Experimental canvas features";
const char kExperimentalCanvasFeaturesDescription[] =
    "Enables the use of experimental canvas features which are still in "
    "development.";

const char kExperimentalExtensionApisName[] = "Experimental Extension APIs";
const char kExperimentalExtensionApisDescription[] =
    "Enables experimental extension APIs. Note that the extension gallery "
    "doesn't allow you to upload extensions that use experimental APIs.";

const char kExperimentalProductivityFeaturesName[] =
    "Experimental Productivity Features";
const char kExperimentalProductivityFeaturesDescription[] =
    "Enable support for experimental developer productivity features, such as "
    "built-in modules and policies for avoiding slow rendering.";

const char kExperimentalSecurityFeaturesName[] =
    "Potentially annoying security features";
const char kExperimentalSecurityFeaturesDescription[] =
    "Enables several security features that will likely break one or more "
    "pages that you visit on a daily basis. Strict mixed content checking, for "
    "example. And locking powerful features to secure contexts. This flag will "
    "probably annoy you.";

const char kExperimentalWebPlatformFeaturesName[] =
    "Experimental Web Platform features";
const char kExperimentalWebPlatformFeaturesDescription[] =
    "Enables experimental Web Platform features that are in development.";

const char kExtensionContentVerificationName[] =
    "Extension Content Verification";
const char kExtensionContentVerificationDescription[] =
    "This flag can be used to turn on verification that the contents of the "
    "files on disk for extensions from the webstore match what they're "
    "expected to be. This can be used to turn on this feature if it would not "
    "otherwise have been turned on, but cannot be used to turn it off (because "
    "this setting can be tampered with by malware).";
const char kExtensionContentVerificationBootstrap[] =
    "Bootstrap (get expected hashes, but do not enforce them)";
const char kExtensionContentVerificationEnforce[] =
    "Enforce (try to get hashes, and enforce them if successful)";
const char kExtensionContentVerificationEnforceStrict[] =
    "Enforce strict (hard fail if we can't get hashes)";

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFeaturePolicyName[] = "Feature Policy";
const char kFeaturePolicyDescription[] =
    "Enables granting and removing access to features through the "
    "Feature-Policy HTTP header.";

const char kFontCacheScalingName[] = "FontCache scaling";
const char kFontCacheScalingDescription[] =
    "Reuse a cached font in the renderer to serve different sizes of font for "
    "faster layout.";

const char kForceEffectiveConnectionTypeName[] =
    "Override effective connection type";
const char kForceEffectiveConnectionTypeDescription[] =
    "Overrides the effective connection type of the current connection "
    "returned by the network quality estimator. Slow 2G on Cellular returns "
    "Slow 2G when connected to a cellular network, and the actual estimate "
    "effective connection type when not on a cellular network. Previews are "
    "usually served on 2G networks.";
const char kEffectiveConnectionTypeUnknownDescription[] = "Unknown";
const char kEffectiveConnectionTypeOfflineDescription[] = "Offline";
const char kEffectiveConnectionTypeSlow2GDescription[] = "Slow 2G";
const char kEffectiveConnectionTypeSlow2GOnCellularDescription[] =
    "Slow 2G On Cellular";
const char kEffectiveConnectionType2GDescription[] = "2G";
const char kEffectiveConnectionType3GDescription[] = "3G";
const char kEffectiveConnectionType4GDescription[] = "4G";

const char kFillOnAccountSelectName[] = "Fill passwords on account selection";
const char kFillOnAccountSelectDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load.";

const char kFillOnAccountSelectHttpName[] =
    "Fill passwords on account selection on HTTP origins";
const char kFillOnAccountSelectHttpDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load on HTTP origins.";

const char kForceTextDirectionName[] = "Force text direction";
const char kForceTextDirectionDescription[] =
    "Explicitly force the per-character directionality of UI text to "
    "left-to-right (LTR) or right-to-left (RTL) mode, overriding the default "
    "direction of the character language.";
const char kForceDirectionLtr[] = "Left-to-right";
const char kForceDirectionRtl[] = "Right-to-left";

const char kForceUiDirectionName[] = "Force UI direction";
const char kForceUiDirectionDescription[] =
    "Explicitly force the UI to left-to-right (LTR) or right-to-left (RTL) "
    "mode, overriding the default direction of the UI language.";

const char kFramebustingName[] =
    "Framebusting requires same-origin or a user gesture";
const char kFramebustingDescription[] =
    "Don't permit an iframe to navigate the top level browsing context unless "
    "they are same-origin or the iframe is processing a user gesture.";

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] =
    "Use GPU to rasterize web content. Requires impl-side painting.";
const char kForceGpuRasterization[] = "Force-enabled for all layers";

const char kGooglePasswordManagerName[] = "Google Password Manager UI";
const char kGooglePasswordManagerDescription[] =
    "Enables access to the Google Password Manager UI from Chrome.";

const char kGoogleProfileInfoName[] = "Google profile name and icon";
const char kGoogleProfileInfoDescription[] =
    "Enables using Google information to populate the profile name and icon in "
    "the avatar menu.";

const char kViewsCastDialogName[] = "Views Cast dialog";
const char kViewsCastDialogDescription[] =
    "Replace the WebUI Cast dialog with a Views toolkit dialog. This requires "
    "#views-browser-windows on Mac. This feature is activated if either this "
    "flag or #upcoming-ui-features is enabled";

const char kHandwritingGestureName[] = "Handwriting Gestures";
const char kHandwritingGestureDescription[] =
    "Enables handwriting gestures within the virtual keyboard";

const char kHardwareMediaKeyHandling[] = "Hardware Media Key Handling";
const char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

const char kHideActiveAppsFromShelfName[] =
    "Hide running apps (that are not pinned) from the shelf";
const char kHideActiveAppsFromShelfDescription[] =
    "Save space in the shelf by hiding running apps (that are not pinned).";

const char kHistoryRequiresUserGestureName[] =
    "New history entries require a user gesture.";
const char kHistoryRequiresUserGestureDescription[] =
    "Require a user gesture to add a history entry.";

const char kHorizontalTabSwitcherAndroidName[] =
    "Enable horizontal tab switcher";
const char kHorizontalTabSwitcherAndroidDescription[] =
    "Changes the layout of the Android tab switcher so tabs scroll "
    "horizontally instead of vertically.";

const char kTabSwitcherOnReturnName[] = "Enable tab switcher on return";
const char kTabSwitcherOnReturnDescription[] =
    "Enable tab switcher on return after specified time has elapsed";

const char kHostedAppQuitNotificationName[] =
    "Quit notification for hosted apps";
const char kHostedAppQuitNotificationDescription[] =
    "Display a notification when quitting Chrome if hosted apps are currently "
    "running.";

const char kHostedAppShimCreationName[] =
    "Creation of app shims for hosted apps on Mac";
const char kHostedAppShimCreationDescription[] =
    "Create app shims on Mac when creating a hosted app.";

const char kHtmlBasedUsernameDetectorName[] = "HTML-based username detector";
const char kHtmlBasedUsernameDetectorDescription[] =
    "Use HTML-based username detector for the password manager.";

const char kIconNtpName[] = "Large icons on the New Tab page";
const char kIconNtpDescription[] =
    "Enable the experimental New Tab page using large icons.";

const char kIgnoreGpuBlacklistName[] = "Override software rendering list";
const char kIgnoreGpuBlacklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kIgnorePreviewsBlacklistName[] = "Ignore Previews Blocklist";
const char kIgnorePreviewsBlacklistDescription[] =
    "Ignore decisions made by the PreviewsBlockList";

const char kImprovedGeoLanguageDataName[] = "Improved Geo-language Data";
const char kImprovedGeoLanguageDataDescription[] =
    "Makes the GeoLanguageModel use higher quality, more refined ULP "
    "geo-language data.";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kJavascriptHarmonyName[] = "Experimental JavaScript";
const char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

const char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
const char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

const char kKeepAliveRendererForKeepaliveRequestsName[] =
    "Keep a renderer alive for keepalive fetch requests";
const char kKeepAliveRendererForKeepaliveRequestsDescription[] =
    "Keep a render process alive when the process has a pending fetch request "
    "with `keepalive' specified.";

const char kLcdTextName[] = "LCD text antialiasing";
const char kLcdTextDescription[] =
    "If disabled, text is rendered with grayscale antialiasing instead of LCD "
    "(subpixel) when doing accelerated compositing.";

const char kLoadMediaRouterComponentExtensionName[] =
    "Load Media Router Component Extension";
const char kLoadMediaRouterComponentExtensionDescription[] =
    "Loads the Media Router component extension at startup.";

const char kLookalikeUrlNavigationSuggestionsName[] =
    "Navigation suggestions for lookalike URLs";
const char kLookalikeUrlNavigationSuggestionsDescription[] =
    "Enable navigation suggestions for URLs that are visually similar to "
    "popular domains or to domains with a site engagement score.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMessageCenterNewStyleNotificationName[] = "New style notification";
const char kMessageCenterNewStyleNotificationDescription[] =
    "Enables the experiment style of material-design notification";

const char kMobileIdentityConsistencyName[] = "Mobile identity consistency";
const char kMobileIdentityConsistencyDescription[] =
    "Enables stronger identity consistency on mobile";

const char kNativeFilesystemAPIName[] = "Native Filesystem API";
const char kNativeFilesystemAPIDescription[] =
    "Enables the experimental Native Filesystem API, giving websites access to "
    "the native filesystem";

const char kNewAudioRenderingMixingStrategyName[] =
    "New audio rendering mixing strategy";
const char kNewAudioRenderingMixingStrategyDescription[] =
    "Use the new audio rendering mixing strategy.";

const char kNewBookmarkAppsName[] = "The new bookmark app system";
const char kNewBookmarkAppsDescription[] =
    "Enables the new system for creating bookmark apps.";

const char kNewPasswordFormParsingName[] =
    "New password form parsing for filling passwords";
const char kNewPasswordFormParsingDescription[] =
    "Replaces existing form parsing for filling in password manager with a new "
    "version, currently under development. WARNING: when enabled, Password "
    "Manager might stop working";

const char kNewPasswordFormParsingForSavingName[] =
    "New password form parsing for saving passwords";
const char kNewPasswordFormParsingForSavingDescription[] =
    "Replaces existing form parsing for saving in password manager with a new "
    "version, currently under development. WARNING: when enabled, Password "
    "Manager might stop working";

const char kUseSurfaceLayerForVideoName[] =
    "Enable the use of SurfaceLayer objects for videos.";
const char kUseSurfaceLayerForVideoDescription[] =
    "Enable compositing onto a Surface instead of a VideoLayer "
    "for videos.";

const char kNewUsbBackendName[] = "Enable new USB backend";
const char kNewUsbBackendDescription[] =
    "Enables the new experimental USB backend for Windows.";

const char kNewblueName[] = "Newblue";
const char kNewblueDescription[] =
    "Enables the use of newblue Bluetooth daemon.";

const char kNewTabLoadingAnimation[] = "New tab-loading animation";
const char kNewTabLoadingAnimationDescription[] =
    "Enables a new look for the tab-loading animation.";

const char kNewTabButtonPosition[] = "New tab button position";
const char kNewTabButtonPositionDescription[] =
    "Controls placement of the new tab button within the tabstrip.";
const char kNewTabButtonPositionOppositeCaption[] = "Opposite caption buttons";
const char kNewTabButtonPositionLeading[] = "Leading";
const char kNewTabButtonPositionAfterTabs[] = "After tabs";
const char kNewTabButtonPositionTrailing[] = "Trailing";

const char kNostatePrefetchName[] = "NoState Prefetch";
const char kNostatePrefetchDescription[] =
    "If enabled, pre-downloads resources to improve page load speed.";

const char kNotificationIndicatorName[] = "Notification Indicators";
const char kNotificationIndicatorDescription[] =
    "Enable notification indicators, which appear on app icons when a "
    "notification is active. This will also enable notifications in context "
    "menus.";

const char kNotificationsNativeFlagName[] = "Enable native notifications.";
const char kNotificationsNativeFlagDescription[] =
    "Enable support for using the native notification toasts and notification "
    "center on platforms where these are available.";

const char kUseMultiloginEndpointName[] = "Use Multilogin endpoint.";
const char kUseMultiloginEndpointDescription[] =
    "Use Gaia OAuth multilogin for identity consistency.";

#if defined(OS_POSIX)
const char kNtlmV2EnabledName[] = "Enable NTLMv2 Authentication";
const char kNtlmV2EnabledDescription[] =
    "Enable NTLMv2 HTTP Authentication. This disables NTLMv1 support.";
#endif

const char kOfferStoreUnmaskedWalletCardsName[] =
    "Google Payments card saving checkbox";
const char kOfferStoreUnmaskedWalletCardsDescription[] =
    "Show the checkbox to offer local saving of a credit card downloaded from "
    "the server.";

const char kOfflineAutoReloadName[] = "Offline Auto-Reload Mode";
const char kOfflineAutoReloadDescription[] =
    "Pages that fail to load while the browser is offline will be "
    "auto-reloaded when the browser is online again.";

const char kOfflineAutoReloadVisibleOnlyName[] =
    "Only Auto-Reload Visible Tabs";
const char kOfflineAutoReloadVisibleOnlyDescription[] =
    "Pages that fail to load while the browser is offline will only be "
    "auto-reloaded if their tab is visible.";

const char kOmniboxDisplayTitleForCurrentUrlName[] =
    "Include title for the current URL in the omnibox";
const char kOmniboxDisplayTitleForCurrentUrlDescription[] =
    "In the event that the omnibox provides suggestions on-focus, the URL of "
    "the current page is provided as the first suggestion without a title. "
    "Enabling this flag causes the title to be displayed.";

const char kOmniboxMaterialDesignWeatherIconsName[] =
    "Omnibox Material Design Weather Icons";
const char kOmniboxMaterialDesignWeatherIconsDescription[] =
    "Use material design weather icons in the omnibox when displaying weather "
    "answers.";

const char kOmniboxNewAnswerLayoutName[] = "Omnibox new answer layout";
const char kOmniboxNewAnswerLayoutDescription[] =
    "Modernize omnibox answers using an enhanced layout with larger icons.";

const char kOmniboxRichEntitySuggestionsName[] =
    "Omnibox rich entity suggestions";
const char kOmniboxRichEntitySuggestionsDescription[] =
    "Display entity suggestions using images and an enhanced layout; showing "
    "more context and descriptive text about the entity. Has no effect unless "
    "either the #upcoming-ui-features flag is Enabled or the #top-chrome-md "
    "flag is set to Refresh or Touchable Refresh.";

const char kOmniboxSpareRendererName[] =
    "Start spare renderer on omnibox focus";
const char kOmniboxSpareRendererDescription[] =
    "When the omnibox is focused, start an empty spare renderer. This can "
    "speed up the load of the navigation from the omnibox.";

const char kOmniboxUIBlueSearchLoopAndSearchQueryName[] =
    "Omnibox UI Blue Search Loop and Search Query";
const char kOmniboxUIBlueSearchLoopAndSearchQueryDescription[] =
    "Color the generic search icon and search terms blue.";

const char kOmniboxUIBlueTitlesAndGrayUrlsOnPageSuggestionsName[] =
    "Omnibox UI Blue Titles And Gray Urls On Page Suggestions";
const char kOmniboxUIBlueTitlesAndGrayUrlsOnPageSuggestionsDescription[] =
    "Displays navigation suggestions with blue titles and gray URLs.";

const char kOmniboxUIBlueTitlesOnPageSuggestionsName[] =
    "Omnibox UI Blue Titles On Page Suggestions";
const char kOmniboxUIBlueTitlesOnPageSuggestionsDescription[] =
    "Displays navigation suggestions with blue titles.";

const char kOmniboxUIBoldUserTextOnSearchSuggestionsName[] =
    "Omnibox UI Bold User Text On Search Suggestions";
const char kOmniboxUIBoldUserTextOnSearchSuggestionsDescription[] =
    "Bolds the user text instead of autocomplete text for search suggestions.";

const char kOmniboxUIHideSteadyStateUrlSchemeName[] =
    "Omnibox UI Hide Steady-State URL Scheme";
const char kOmniboxUIHideSteadyStateUrlSchemeDescription[] =
    "In the omnibox, hide the scheme from steady state displayed URLs. It is "
    "restored during editing.";

const char kOmniboxUIOneClickUnelideName[] = "Omnibox UI One Click Unelide";
const char kOmniboxUIOneClickUnelideDescription[] =
    "In the omnibox, undo all unelisions with a single click or focus action.";

const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[] =
    "Omnibox UI Hide Steady-State URL Trivial Subdomains";
const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[] =
    "In the omnibox, hide trivial subdomains from steady state displayed URLs. "
    "Hidden portions are restored during editing.";

const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefName[] =
    "Omnibox UI Hide Steady-State URL Path, Query, and Ref";
const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription[] =
    "In the omnibox, hide the path, query and ref from steady state displayed "
    "URLs. Hidden portions are restored during editing.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";

const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxUIShowSuffixOnAllSearchSuggestionsName[] =
    "Omnibox UI Show Suffix On All Search Suggestions";
const char kOmniboxUIShowSuffixOnAllSearchSuggestionsDescription[] =
    "Shows the search suffix (e.g. \" - Google Search\") on all search "
    "suggestions in the omnibox.";

const char kOmniboxUIShowSuggestionFaviconsName[] =
    "Omnibox UI Show Suggestion Favicons";
const char kOmniboxUIShowSuggestionFaviconsDescription[] =
    "Shows favicons instead of generic vector icons for URL suggestions in the "
    "Omnibox dropdown.";

const char kOmniboxUISwapTitleAndUrlName[] = "Omnibox UI Swap Title and URL";
const char kOmniboxUISwapTitleAndUrlDescription[] =
    "In the omnibox dropdown, shows titles before URLs when both are "
    "available.";

const char kOmniboxUIUnboldSuggestionTextName[] =
    "Omnibox UI Unbold Suggestion Text";
const char kOmniboxUIUnboldSuggestionTextDescription[] =
    "Displays search suggestions without bolding.";

const char kOmniboxUIUseGenericSearchEngineIconName[] =
    "Omnibox UI Use Generic Search Engine Icon";
const char kOmniboxUIUseGenericSearchEngineIconDescription[] =
    "In the omnibox and NTP, shows a generic magnifying glass icon instead of  "
    "the Search Provider favicon.";

const char kOmniboxUIVerticalMarginName[] = "Omnibox UI Vertical Margin";
const char kOmniboxUIVerticalMarginDescription[] =
    "Changes the vertical margin in the Omnibox UI.";

const char kOmniboxUIWhiteBackgroundOnBlurName[] =
    "Omnibox UI White Background On Blur";
const char kOmniboxUIWhiteBackgroundOnBlurDescription[] =
    "Set the omnibox background white when it's unfocused";

const char kOmniboxVoiceSearchAlwaysVisibleName[] =
    "Omnibox Voice Search Always Visible";
const char kOmniboxVoiceSearchAlwaysVisibleDescription[] =
    "Always displays voice search icon in focused omnibox as long as voice "
    "search is possible";

const char kOnlyNewPasswordFormParsingName[] =
    "Use only new password form parsing";
const char kOnlyNewPasswordFormParsingDescription[] =
    "The old password form parsing is disabled";

const char kOnTheFlyMhtmlHashComputationName[] =
    "On-The-Fly MHTML Hash Computation";
const char kOnTheFlyMhtmlHashComputationDescription[] =
    "Save MHTML files to the target location and calculate their content "
    "digests in one step.";

const char kOopRasterizationName[] = "Out of process rasterization";
const char kOopRasterizationDescription[] =
    "Perform Ganesh raster in the GPU Process instead of the renderer.  "
    "Must also enable GPU rasterization";

const char kOriginTrialsName[] = "Origin Trials";
const char kOriginTrialsDescription[] =
    "Enables origin trials for controlling access to feature/API experiments.";

const char kOverlayScrollbarsName[] = "Overlay Scrollbars";
const char kOverlayScrollbarsDescription[] =
    "Enable the experimental overlay scrollbars implementation. You must also "
    "enable threaded compositing to have the scrollbars animate.";

const char kOverlayScrollbarsFlashAfterAnyScrollUpdateName[] =
    "Flash Overlay Scrollbars After Any Scroll Update";
const char kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription[] =
    "Flash Overlay Scrollbars After any scroll update happends in page. You"
    " must also enable Overlay Scrollbars.";

const char kOverlayScrollbarsFlashWhenMouseEnterName[] =
    "Flash Overlay Scrollbars When Mouse Enter";
const char kOverlayScrollbarsFlashWhenMouseEnterDescription[] =
    "Flash Overlay Scrollbars When Mouse Enter a scrollable area. You must also"
    " enable Overlay Scrollbars.";

const char kOverlayStrategiesName[] = "Select HW overlay strategies";
const char kOverlayStrategiesDescription[] =
    "Select strategies used to promote quads to HW overlays.";
const char kOverlayStrategiesDefault[] = "Default";
const char kOverlayStrategiesNone[] = "None";
const char kOverlayStrategiesUnoccludedFullscreen[] =
    "Unoccluded fullscreen buffers (single-fullscreen)";
const char kOverlayStrategiesUnoccluded[] =
    "Unoccluded buffers (single-fullscreen,single-on-top)";
const char kOverlayStrategiesOccludedAndUnoccluded[] =
    "Occluded and unoccluded buffers "
    "(single-fullscreen,single-on-top,underlay)";

const char kUseNewAcceptLanguageHeaderName[] = "Use new Accept-Language header";
const char kUseNewAcceptLanguageHeaderDescription[] =
    "Adds the base language code after other corresponding language+region "
    "codes. This ensures that users receive content in their preferred "
    "language.";

const char kOverscrollHistoryNavigationName[] = "Overscroll history navigation";
const char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

const char kOverscrollStartThresholdName[] = "Overscroll start threshold";
const char kOverscrollStartThresholdDescription[] =
    "Changes overscroll start threshold relative to the default value.";
const char kOverscrollStartThreshold133Percent[] = "133%";
const char kOverscrollStartThreshold166Percent[] = "166%";
const char kOverscrollStartThreshold200Percent[] = "200%";

const char kTouchpadOverscrollHistoryNavigationName[] =
    "Overscroll history navigation on Touchpad";
const char kTouchpadOverscrollHistoryNavigationDescription[] =
    "Allows swipe left/right from touchpad change browser navigation.";

const char kParallelDownloadingName[] = "Parallel downloading";
const char kParallelDownloadingDescription[] =
    "Enable parallel downloading to accelerate download speed.";

const char kPassiveEventListenerDefaultName[] =
    "Passive Event Listener Override";
const char kPassiveEventListenerDefaultDescription[] =
    "Forces touchstart, touchmove, mousewheel and wheel event listeners (which "
    "haven't requested otherwise) to be treated as passive. This will break "
    "touch/wheel behavior on some websites but is useful for demonstrating the "
    "potential performance benefits of adopting passive event listeners.";
const char kPassiveEventListenerTrue[] = "True (when unspecified)";
const char kPassiveEventListenerForceAllTrue[] = "Force All True";

const char kPassiveEventListenersDueToFlingName[] =
    "Touch Event Listeners Passive Default During Fling";
const char kPassiveEventListenersDueToFlingDescription[] =
    "Forces touchstart, and first touchmove per scroll event listeners during "
    "fling to be treated as passive.";

const char kPassiveDocumentEventListenersName[] =
    "Document Level Event Listeners Passive Default";
const char kPassiveDocumentEventListenersDescription[] =
    "Forces touchstart, and touchmove event listeners on document level "
    "targets (which haven't requested otherwise) to be treated as passive.";

const char kPassiveDocumentWheelEventListenersName[] =
    "Document Level Wheel Event Listeners Passive Default";
const char kPassiveDocumentWheelEventListenersDescription[] =
    "Forces wheel, and mousewheel event listeners on document level targets "
    "(which haven't requested otherwise) to be treated as passive.";

const char kPasswordImportName[] = "Password import";
const char kPasswordImportDescription[] =
    "Import functionality in password settings.";

const char kPasswordsKeyboardAccessoryName[] =
    "Add password-related functions to keyboard accessory";
const char kPasswordsKeyboardAccessoryDescription[] =
    "Adds password generation button and toggle for the passwords bottom sheet "
    "to the keyboard accessory. Replaces password generation popups.";

const char kPasswordsMigrateLinuxToLoginDBName[] =
    "Migrate passwords to \"Login Data\"";
const char kPasswordsMigrateLinuxToLoginDBDescription[] =
    "Performs a one-off irreversible migration of passwords from the "
    "gnome-keyring or kwallet into the profile directory.";

const char kPerMethodCanMakePaymentQuotaName[] =
    "Per-method canMakePayment() quota.";
const char kPerMethodCanMakePaymentQuotaDescription[] =
    "Allow calling canMakePayment() for different payment methods, as long as "
    "method-specific parameters remain unchanged.";

const char kPinchScaleName[] = "Pinch scale";
const char kPinchScaleDescription[] =
    "Enables experimental support for scale using pinch.";

const char kPreviewsAllowedName[] = "Previews Allowed";
const char kPreviewsAllowedDescription[] =
    "Allows previews to be shown subject to specific preview types being "
    "enabled and the client experiencing specific triggering conditions. "
    "May be used as a kill-switch to turn off all potential preview types.";

const char kPrintPdfAsImageName[] = "Print Pdf as Image";
const char kPrintPdfAsImageDescription[] =
    "If enabled, an option to print PDF files as images will be available in "
    "print preview.";

const char kPrintPreviewRegisterPromosName[] =
    "Print Preview Registration Promos";
const char kPrintPreviewRegisterPromosDescription[] =
    "Enable registering unregistered cloud printers from print preview.";

const char kPullToRefreshName[] = "Pull-to-refresh gesture";
const char kPullToRefreshDescription[] =
    "Pull-to-refresh gesture in response to vertical overscroll.";
const char kPullToRefreshEnabledTouchscreen[] = "Enabled for touchscreen only";

const char kQueryInOmniboxName[] = "Query in Omnibox";
const char kQueryInOmniboxDescription[] =
    "Only display query terms in the omnibox when viewing a search results "
    "page.";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kRecurrentInterstitialName[] =
    "Show a message when the same SSL error recurs";
const char kRecurrentInterstitialDescription[] =
    "Enable a special message on the SSL certificate error page when the same "
    "SSL error occurs repeatedly.";

const char kReducedReferrerGranularityName[] =
    "Reduce default 'referer' header granularity.";
const char kReducedReferrerGranularityDescription[] =
    "If a page hasn't set an explicit referrer policy, setting this flag will "
    "reduce the amount of information in the 'referer' header for cross-origin "
    "requests.";

const char kRewriteLevelDBOnDeletionName[] =
    "Rewrite LevelDB instances after full deletions";
const char kRewriteLevelDBOnDeletionDescription[] =
    "Rewrite LevelDB instances to remove traces of deleted data from disk.";

const char kRendererSideResourceSchedulerName[] =
    "Renderer side ResourceScheduler";
const char kRendererSideResourceSchedulerDescription[] =
    "Migrate some ResourceScheduler functionalities to renderer";

const char kRequestTabletSiteName[] =
    "Request tablet site option in the settings menu";
const char kRequestTabletSiteDescription[] =
    "Allows the user to request tablet site. Web content is often optimized "
    "for tablet devices. When this option is selected the user agent string is "
    "changed to indicate a tablet device. Web content optimized for tablets is "
    "received there after for the current tab.";

const char kResetAppListInstallStateName[] =
    "Reset the App Launcher install state on every restart.";
const char kResetAppListInstallStateDescription[] =
    "Reset the App Launcher install state on every restart. While this flag is "
    "set, Chrome will forget the launcher has been installed each time it "
    "starts. This is used for testing the App Launcher install flow.";

const char kResourceLoadSchedulerName[] = "Enable resource load throttling";
const char kResourceLoadSchedulerDescription[] =
    "Uses the resource load scheduler in blink to throttle resource load "
    "requests.";

const char kSafeBrowsingUseAPDownloadVerdictsName[] =
    "Request Advanced Protection verdicts when inspecting downloads";
const char kSafeBrowsingUseAPDownloadVerdictsDescription[] =
    "If enabled, download protection will request Advanced Protection "
    "verdicts from Safe Browsing. These will provide stronger protections "
    "from files Safe Browsing is unsure about.";

const char kSafeSearchUrlReportingName[] = "SafeSearch URLs reporting.";
const char kSafeSearchUrlReportingDescription[] =
    "If enabled, inappropriate URLs can be reported back to SafeSearch.";

const char kSamplingHeapProfilerName[] = "Native memory sampling profiler.";
const char kSamplingHeapProfilerDescription[] =
    "Enables native memory sampling profiler with specified rate in KiB. "
    "If sampling rate is not provided the default value of 128 KiB is used.";

const char kSaveasMenuLabelExperimentName[] =
    "Switch 'Save as' menu labels to 'Download'";
const char kSaveasMenuLabelExperimentDescription[] =
    "Enables an experiment to switch menu labels that use 'Save as...' to "
    "'Download'.";

const char kSavePageAsMhtmlName[] = "Save Page as MHTML";
const char kSavePageAsMhtmlDescription[] =
    "Enables saving pages as MHTML: a single text file containing HTML and all "
    "sub-resources.";

const char kSendTabToSelfName[] = "Send tab to self";
const char kSendTabToSelfDescription[] =
    "Allows users to push tabs from Android devices to other synced "
    "devices, in order to easily transition those tabs to the new device ";

const char kServiceWorkerImportedScriptUpdateCheckName[] =
    "Enable update check for service worker importScripts() resources";
const char kServiceWorkerImportedScriptUpdateCheckDescription[] =
    "Extend byte-for-byte update check for scripts that are imported by the "
    "service worker script via importScripts().";

const char kServiceWorkerLongRunningMessageName[] =
    "Service worker long running message dispatch.";
const char kServiceWorkerLongRunningMessageDescription[] =
    "Enables long running message dispatch method for service workers. "
    "Messages sent with this method do not timeout, allowing the service "
    "worker to run indefinitely.";

const char kSessionRestorePrioritizesBackgroundUseCasesName[] =
    "Session restore prioritizes background use cases.";
const char kSessionRestorePrioritizesBackgroundUseCasesDescription[] =
    "When enabled session restore logic will prioritize sites that make use of "
    "background communication mechanisms (favicon and tab title switches, "
    "notifications, etc) over sites that do not.";

const char kSettingsWindowName[] = "Show settings in a window";
const char kSettingsWindowDescription[] =
    "Settings will be shown in a dedicated window instead of as a browser tab.";

const char kShelfHoverPreviewsName[] =
    "Show previews of running apps when hovering over the shelf.";
const char kShelfHoverPreviewsDescription[] =
    "Shows previews of the open windows for a given running app when hovering "
    "over the shelf.";

const char kShowAndroidFilesInFilesAppName[] =
    "Show Android files in Files app";
const char kShowAndroidFilesInFilesAppDescription[] =
    "Show Android files in Files app if Android is enabled on the device.";

const char kShowAutofillSignaturesName[] = "Show autofill signatures.";
const char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
const char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

const char kHistoryManipulationIntervention[] =
    "History Manipulation Intervention";
const char kHistoryManipulationInterventionDescription[] =
    "If a page does a client side redirect or adds to the history without a "
    "user gesture, then skip it on back/forward UI.";

const char kSupervisedUserCommittedInterstitialsName[] =
    "Enable Supervised User Committed Interstitials";
const char kSupervisedUserCommittedInterstitialsDescription[] =
    "Use committed error pages instead of transient navigation entries for "
    "supervised user interstitials";

const char kSilentDebuggerExtensionApiName[] = "Silent Debugging";
const char kSilentDebuggerExtensionApiDescription[] =
    "Do not show the infobar when an extension attaches to a page via "
    "chrome.debugger API. This is required to debug extension background "
    "pages.";

const char kSignedHTTPExchangeName[] = "Signed HTTP Exchange";
const char kSignedHTTPExchangeDescription[] =
    "Enables Origin-Signed HTTP Exchanges support which is still in "
    "development. Warning: Enabling this may pose a security risk.";

const char kSimplifyHttpsIndicatorName[] = "Simplify HTTPS indicator UI";
const char kSimplifyHttpsIndicatorDescription[] =
    "Change the UI treatment for HTTPS pages.";

const char kSiteIsolationOptOutName[] = "Disable site isolation";
const char kSiteIsolationOptOutDescription[] =
    "Disables site isolation "
    "(SitePerProcess, IsolateOrigins, etc). Intended for diagnosing bugs that "
    "may be due to out-of-process iframes. Opt-out has no effect if site "
    "isolation is force-enabled using a command line switch or using an "
    "enterprise policy. "
    "Caution: this disables important mitigations for the Spectre CPU "
    "vulnerability affecting most computers.";
const char kSiteIsolationOptOutChoiceDefault[] = "Default";
const char kSiteIsolationOptOutChoiceOptOut[] = "Disabled (not recommended)";

const char kSiteSettings[] = "Site settings";
const char kSiteSettingsDescription[] =
    "Add the All Sites list to Site Settings.";

const char kSmoothScrollingName[] = "Smooth Scrolling";
const char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

const char kSoleIntegrationName[] = "Sole integration";
const char kSoleIntegrationDescription[] =
    "Enable Sole integration for browser customization. You must restart "
    "the browser twice for changes to take effect.";

const char kSpeculativeServiceWorkerStartOnQueryInputName[] =
    "Enable speculative start of a service worker when a search is predicted.";
const char kSpeculativeServiceWorkerStartOnQueryInputDescription[] =
    "If enabled, when the user enters text in the omnibox that looks like a "
    "a query, any service worker associated with the search engine the query "
    "will be sent to is started early.";

const char kSpellingFeedbackFieldTrialName[] = "Spelling Feedback Field Trial";
const char kSpellingFeedbackFieldTrialDescription[] =
    "Enable the field trial for sending user feedback to spelling service.";

const char kSSLCommittedInterstitialsName[] = "Committed Interstitials";
const char kSSLCommittedInterstitialsDescription[] =
    "Use committed error pages instead of transient navigation entries "
    "for SSL interstitial error pages (i.e. certificate errors).";

const char kStopInBackgroundName[] = "Stop in background";
const char kStopInBackgroundDescription[] =
    "Stop scheduler task queues, in the background, "
    " after a grace period.";

const char kStopNonTimersInBackgroundName[] =
    "Stop non-timer task queues background";
const char kStopNonTimersInBackgroundDescription[] =
    "Stop non-timer task queues, in the background, "
    "after a grace period.";

const char kSuggestionsWithSubStringMatchName[] =
    "Substring matching for Autofill suggestions";
const char kSuggestionsWithSubStringMatchDescription[] =
    "Match Autofill suggestions based on substrings (token prefixes) rather "
    "than just prefixes.";

const char kSyncSupportSecondaryAccountName[] =
    "Support secondary accounts for Sync standalone transport";
const char kSyncSupportSecondaryAccountDescription[] =
    "If enabled, allows Chrome Sync to start in standalone transport mode for "
    "a signed-in account that has not been chosen as Chrome's primary account. "
    "This only has an effect if sync-standalone-transport is also enabled.";

const char kSyncUSSAutofillProfileName[] = "Enable USS for autofill profile";
const char kSyncUSSAutofillProfileDescription[] =
    "Enables the new implementation of autofill profile sync";

const char kSyncUSSAutofillWalletDataName[] =
    "Enable USS for autofill wallet data";
const char kSyncUSSAutofillWalletDataDescription[] =
    "Enables the new implementation of autofill walet data sync";

const char kSyncUSSAutofillWalletMetadataName[] =
    "Enable USS for autofill wallet metadata";
const char kSyncUSSAutofillWalletMetadataDescription[] =
    "Enables the new implementation of autofill walet metadata sync";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSystemKeyboardLockName[] = "Experimental system keyboard lock";
const char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

const char kTabGridLayoutAndroidName[] = "Tab Grid Layout";
const char kTabGridLayoutAndroidDescription[] =
    "Allows users to see their tabs in a grid layout in the tab switcher.";

const char kTabGroupsName[] = "Tab Groups";
const char kTabGroupsDescription[] =
    "Allows users to organize tabs into visually distinct groups, e.g. to "
    "separate tabs associated with different tasks.";

const char kTabHoverCardsName[] = "Tab Hover Cards";
const char kTabHoverCardsDescription[] =
    "Enables a popup containing tab information to be visible when hovering "
    "over a tab. This will replace tooltips for tabs.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

const char kTabsInCbdName[] = "Enable tabs for the Clear Browsing Data dialog.";
const char kTabsInCbdDescription[] =
    "Enables a basic and an advanced tab for the Clear Browsing Data dialog.";

const char kTintGlCompositedContentName[] = "Tint GL-composited content";
const char kTintGlCompositedContentDescription[] =
    "Tint contents composited using GL with a shade of red to help debug and "
    "study overlay support.";

const char kTopChromeTouchUiName[] = "Touch UI Layout";
const char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

const char kThreadedScrollingName[] = "Threaded scrolling";
const char kThreadedScrollingDescription[] =
    "Threaded handling of scroll-related input events. Disabling this will "
    "force all such scroll events to be handled on the main thread. Note that "
    "this can dramatically hurt scrolling performance of most websites and is "
    "intended for testing purposes only.";

const char kTopSitesFromSiteEngagementName[] = "Top Sites from Site Engagement";
const char kTopSitesFromSiteEngagementDescription[] =
    "Enable Top Sites on the New Tab Page to be sourced and sorted using site "
    "engagement.";

const char kTouchAdjustmentName[] = "Touch adjustment";
const char kTouchAdjustmentDescription[] =
    "Refine the position of a touch gesture in order to compensate for touches "
    "having poor resolution compared to a mouse.";

const char kTouchDragDropName[] = "Touch initiated drag and drop";
const char kTouchDragDropDescription[] =
    "Touch drag and drop can be initiated through long press on a draggable "
    "element.";

const char kTouchEventsName[] = "Touch Events API";
const char kTouchEventsDescription[] =
    "Force Touch Events API feature detection to always be enabled or "
    "disabled, or to be enabled when a touchscreen is detected on startup "
    "(Automatic).";

const char kTouchSelectionStrategyName[] = "Touch text selection strategy";
const char kTouchSelectionStrategyDescription[] =
    "Controls how text selection granularity changes when touch text selection "
    "handles are dragged. Non-default behavior is experimental.";
const char kTouchSelectionStrategyCharacter[] = "Character";
const char kTouchSelectionStrategyDirection[] = "Direction";

const char kTraceUploadUrlName[] = "Trace label for navigation tracing";
const char kTraceUploadUrlDescription[] =
    "This is to be used in conjunction with the enable-navigation-tracing "
    "flag. Please select the label that best describes the recorded traces. "
    "This will choose the destination the traces are uploaded to. If you are "
    "not sure, select other. If left empty, no traces will be uploaded.";
const char kTraceUploadUrlChoiceOther[] = "Other";
const char kTraceUploadUrlChoiceEmloading[] = "emloading";
const char kTraceUploadUrlChoiceQa[] = "QA";
const char kTraceUploadUrlChoiceTesting[] = "Testing";

const char kTranslateForceTriggerOnEnglishName[] =
    "Select which language model to use to trigger translate on English "
    "content";
const char kTranslateForceTriggerOnEnglishDescription[] =
    "Force the Translate Triggering on English pages experiment to be enabled "
    "with the selected language model active.";

const char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
const char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. For the definition of secure "
    "contexts, "
    "see https://w3c.github.io/webappsec-secure-contexts/";

const char kTrySupportedChannelLayoutsName[] =
    "Causes audio output streams to check if channel layouts other than the "
    "default hardware layout are available.";
const char kTrySupportedChannelLayoutsDescription[] =
    "Causes audio output streams to check if channel layouts other than the "
    "default hardware layout are available. Turning this on will allow the OS "
    "to do stereo to surround expansion if supported. May expose third party "
    "driver bugs, use with caution.";

const char kUnifiedConsentName[] = "Unified Consent";
const char kUnifiedConsentDescription[] =
    "Enables a unified management of user consent for privacy-related "
    "features. This includes new confirmation screens and improved settings "
    "pages.";

const char kUiPartialSwapName[] = "Partial swap";
const char kUiPartialSwapDescription[] = "Sets partial swap behavior.";

const char kUsePdfCompositorServiceName[] =
    "Use PDF compositor service for printing";
const char kUsePdfCompositorServiceDescription[] =
    "When enabled, use PDF compositor service to composite and generate PDF "
    "files for printing. When site isolation is enabled, disabling this will "
    "not stop using PDF compositor service since the service is required for "
    "printing out-of-process iframes correctly.";

const char kUserActivationV2Name[] = "User Activation v2";
const char kUserActivationV2Description[] =
    "Enable simple user activation for APIs that are otherwise controlled by "
    "user gesture tokens.";

const char kUserConsentForExtensionScriptsName[] =
    "User consent for extension scripts";
const char kUserConsentForExtensionScriptsDescription[] =
    "Require user consent for an extension running a script on the page, if "
    "the extension requested permission to run on all urls.";

const char kUseSuggestionsEvenIfFewFeatureName[] =
    "Disable minimum for server-side tile suggestions on NTP.";
const char kUseSuggestionsEvenIfFewFeatureDescription[] =
    "Request server-side suggestions even if there are only very few of them "
    "and use them for tiles on the New Tab Page.";

const char kV8CacheOptionsName[] = "V8 caching mode.";
const char kV8CacheOptionsDescription[] =
    "Caching mode for the V8 JavaScript engine.";
const char kV8CacheOptionsCode[] = "Cache V8 compiler data.";

const char kV8VmFutureName[] = "Future V8 VM features";
const char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

const char kV8OrinocoName[] = "V8 Orinoco garbage collection features";
const char kV8OrinocoDescription[] =
    "This enables the V8 Orinoco garbage collection features.";

const char kVideoFullscreenOrientationLockName[] =
    "Lock screen orientation when playing a video fullscreen.";
const char kVideoFullscreenOrientationLockDescription[] =
    "Lock the screen orientation of the device to match video orientation when "
    "a video goes fullscreen. Only on phones.";

const char kVideoRotateToFullscreenName[] =
    "Rotate-to-fullscreen gesture for videos.";
const char kVideoRotateToFullscreenDescription[] =
    "Enter/exit fullscreen when device is rotated to/from the orientation of "
    "the video. Only on phones.";

const char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
const char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

const char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
const char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "Extensions that are still in draft status.";

const char kWebMidiName[] = "Web MIDI API";
const char kWebMidiDescription[] = "Enable Web MIDI API experimental support.";

const char kWebrtcH264WithOpenh264FfmpegName[] =
    "WebRTC H.264 software video encoder/decoder";
const char kWebrtcH264WithOpenh264FfmpegDescription[] =
    "When enabled, an H.264 software video encoder/decoder pair is included. "
    "If a hardware encoder/decoder is also available it may be used instead of "
    "this encoder/decoder.";

const char kWebrtcHideLocalIpsWithMdnsName[] =
    "Anonymize local IPs exposed by WebRTC.";
const char kWebrtcHideLocalIpsWithMdnsDecription[] =
    "Conceal local IP addresses with mDNS hostnames.";

const char kWebrtcHybridAgcName[] = "WebRTC hybrid Agc2/Agc1.";
const char kWebrtcHybridAgcDescription[] =
    "WebRTC Agc2 digital adaptation with Agc1 analog adaptation.";

const char kWebrtcHwDecodingName[] = "WebRTC hardware video decoding";
const char kWebrtcHwDecodingDescription[] =
    "Support in WebRTC for decoding video streams using platform hardware.";

const char kWebrtcHwEncodingName[] = "WebRTC hardware video encoding";
const char kWebrtcHwEncodingDescription[] =
    "Support in WebRTC for encoding video streams using platform hardware.";

const char kWebrtcHwH264EncodingName[] = "WebRTC hardware h264 video encoding";
const char kWebrtcHwH264EncodingDescription[] =
    "Support in WebRTC for encoding h264 video streams using platform "
    "hardware.";

const char kWebrtcHwVP8EncodingName[] = "WebRTC hardware vp8 video encoding";
const char kWebrtcHwVP8EncodingDescription[] =
    "Support in WebRTC for encoding vp8 video streams using platform hardware.";

const char kWebrtcHwVP9EncodingName[] = "WebRTC hardware vp9 video encoding";
const char kWebrtcHwVP9EncodingDescription[] =
    "Support in WebRTC for encoding vp9 video streams using platform hardware.";

const char kWebrtcNewEncodeCpuLoadEstimatorName[] =
    "WebRTC new encode cpu load estimator";
const char kWebrtcNewEncodeCpuLoadEstimatorDescription[] =
    "Enable new estimator for the encoder cpu load, for evaluation and "
    "testing. Intended to improve accuracy when screen casting.";

const char kWebRtcRemoteEventLogName[] = "WebRTC remote-bound event logging";
const char kWebRtcRemoteEventLogDescription[] =
    "Allow collecting WebRTC event logs and uploading them to Crash. "
    "Please note that, even if enabled, this will still require "
    "a policy to be set, for it to have an effect.";

const char kWebrtcSrtpAesGcmName[] =
    "Negotiation with GCM cipher suites for SRTP in WebRTC";
const char kWebrtcSrtpAesGcmDescription[] =
    "When enabled, WebRTC will try to negotiate GCM cipher suites for SRTP.";

const char kWebrtcSrtpEncryptedHeadersName[] =
    "Negotiation with encrypted header extensions for SRTP in WebRTC";
const char kWebrtcSrtpEncryptedHeadersDescription[] =
    "When enabled, WebRTC will try to negotiate encrypted header extensions "
    "for SRTP.";

const char kWebrtcStunOriginName[] = "WebRTC Stun origin header";
const char kWebrtcStunOriginDescription[] =
    "When enabled, Stun messages generated by WebRTC will contain the Origin "
    "header.";

const char kWebvrName[] = "WebVR";
const char kWebvrDescription[] =
    "Enables access to experimental Virtual Reality functionality via the "
    "WebVR 1.1 API. This feature will eventually be replaced by the WebXR "
    "Device API. Warning: Enabling this will also allow WebVR content on "
    "insecure origins to access these powerful APIs, and may pose a security "
    "risk. Controllers are exposed as Gamepads, and WebVR-specific attributes "
    "are exposed.";

const char kWebXrName[] = "WebXR Device API";
const char kWebXrDescription[] =
    "Enables access to experimental APIs to interact with Virtual Reality (VR) "
    "and Augmented Reality (AR) devices.";

const char kWebXrGamepadSupportName[] = "WebXR Gamepad Support";
const char kWebXrGamepadSupportDescription[] =
    "Expose VR controllers as Gamepads for use with the WebXR Device API. Each "
    "XRInputSource will have a corresponding Gamepad instance. Requires that "
    "WebXR Device API is also enabled.";

const char kWebXrHitTestName[] = "WebXR Hit Test";
const char kWebXrHitTestDescription[] =
    "Enables access to raycasting against estimated XR scene geometry.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

// Android ---------------------------------------------------------------------

#if defined(OS_ANDROID)

const char kAiaFetchingName[] = "Intermediate Certificate Fetching";
const char kAiaFetchingDescription[] =
    "Enable intermediate certificate fetching when a server does not provide "
    "sufficient certificates to build a chain to a trusted root.";

const char kAccessibilityTabSwitcherName[] = "Accessibility Tab Switcher";
const char kAccessibilityTabSwitcherDescription[] =
    "Enable the accessibility tab switcher for Android.";

const char kAllowRemoteContextForNotificationsName[] =
    "Allow using remote app context for notifications";
const char kAllowRemoteContextForNotificationsDescription[] =
    "Allow using Context#createPackageContext to work around issues with status"
    "bar icons on certain Android M devices.";

const char kAndroidAutofillAccessibilityName[] = "Autofill Accessibility";
const char kAndroidAutofillAccessibilityDescription[] =
    "Enable accessibility for autofill popup.";

const char kAndroidSurfaceControl[] = "Use Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    "Use the SurfaceControl API for supporting overlays on Android";

const char kAndroidWebContentsDarkMode[] = "Android web contents dark mode";
const char kAndroidWebContentsDarkModeDescription[] =
    "Enable dark mode on web contents in Android";

const char kAppNotificationStatusMessagingName[] =
    "App notification status messaging";
const char kAppNotificationStatusMessagingDescription[] =
    "Enables messaging in site permissions UI informing user when "
    "notifications are disabled for the entire app.";

const char kAsyncDnsName[] = "Async DNS resolver";
const char kAsyncDnsDescription[] = "Enables the built-in DNS resolver.";

const char kAutoFetchOnNetErrorPageName[] = "AutoFetchOnNetErrorPage";
const char kAutoFetchOnNetErrorPageDescription[] =
    "When enabled, and navigation fails with an offline error, schedule a "
    "fetch of the page when online again.";

const char kAutofillAccessoryViewName[] =
    "Autofill suggestions as keyboard accessory view";
const char kAutofillAccessoryViewDescription[] =
    "Shows Autofill suggestions on top of the keyboard rather than in a "
    "dropdown.";

const char kBackgroundLoaderForDownloadsName[] =
    "Enables background downloading of pages.";
const char kBackgroundLoaderForDownloadsDescription[] =
    "Enables downloading pages in the background in case page is not yet "
    "loaded in current tab.";

const char kBackgroundTaskComponentUpdateName[] =
    "Background Task Component Updates";
const char kBackgroundTaskComponentUpdateDescription[] =
    "Schedule component updates with BackgroundTaskScheduler";

const char kCCTModuleName[] = "Chrome Custom Tabs Module";
const char kCCTModuleDescription[] =
    "Enables a dynamically loaded module in Chrome Custom Tabs, on Android.";

const char kCCTModuleCacheName[] = "Chrome Custom Tabs Module Cache";
const char kCCTModuleCacheDescription[] =
    "Enables a cache for dynamically loaded modules in Chrome Custom Tabs. "
    "Under mild memory pressure the cache may be retained for some time";

const char kCCTModuleCustomHeaderName[] =
    "Chrome Custom Tabs Module Custom Header";
const char kCCTModuleCustomHeaderDescription[] =
    "Enables header customization by dynamically loaded modules in "
    "Chrome Custom Tabs.";

const char kCCTModuleCustomRequestHeaderName[] =
    "Chrome Custom Tabs Module Custom Request Header";
const char kCCTModuleCustomRequestHeaderDescription[] =
    "Enables a custom request header for URLs managed by dynamically loaded "
    "modules in Chrome Custom Tabs.";

const char kCCTModuleDexLoadingName[] = "Chrome Custom Tabs Module Dex Loading";
const char kCCTModuleDexLoadingDescription[] =
    "Enables loading Chrome Custom Tabs module code from a dex file "
    "provided by the module.";

const char kCCTModulePostMessageName[] =
    "Chrome Custom Tabs Module postMessage API";
const char kCCTModulePostMessageDescription[] =
    "Enables the postMessage API exposed to dynamically loaded modules in "
    "Chrome Custom Tabs.";

const char kCCTModuleUseIntentExtrasName[] =
    "Chrome Custom Tabs Module Intent Extras Usage";
const char kCCTModuleUseIntentExtrasDescription[] =
    "Enables usage of Intent's extras in Chrome Custom Tabs Module";

const char kCCTTargetTranslateLanguageName[] =
    "Chrome Custom Tabs Target Translate Language";
const char kCCTTargetTranslateLanguageDescription[] =
    "Enables specify target language the page should be translated to "
    "in Chrome Custom Tabs.";

const char kChromeDuetName[] = "Chrome Duet";
const char kChromeDuetDescription[] =
    "Enables Chrome Duet, split toolbar Chrome Home, on Android.";

const char kClearOldBrowsingDataName[] = "Clear older browsing data";
const char kClearOldBrowsingDataDescription[] =
    "Enables clearing of browsing data which is older than a given time "
    "period.";

const char kContentSuggestionsCategoryOrderName[] =
    "Default content suggestions category order (e.g. on NTP)";
const char kContentSuggestionsCategoryOrderDescription[] =
    "Set default order of content suggestion categories (e.g. on the NTP).";

const char kContentSuggestionsCategoryRankerName[] =
    "Content suggestions category ranker (e.g. on NTP)";
const char kContentSuggestionsCategoryRankerDescription[] =
    "Set category ranker to order categories of content suggestions (e.g. on "
    "the NTP).";

const char kContentSuggestionsDebugLogName[] = "Content suggestions debug log";
const char kContentSuggestionsDebugLogDescription[] =
    "Enable content suggestions debug log accessible through "
    "snippets-internals.";

const char kContextualSearchDefinitionsName[] = "Contextual Search definitions";
const char kContextualSearchDefinitionsDescription[] =
    "Enables tap-activated contextual definitions of words on a page to be "
    "presented in the caption of the Tap to Search Bar.";

const char kContextualSearchMlTapSuppressionName[] =
    "Contextual Search ML tap suppression";
const char kContextualSearchMlTapSuppressionDescription[] =
    "Enables tap gestures to be suppressed to improve CTR by applying machine "
    "learning.  The \"Contextual Search Ranker prediction\" flag must also be "
    "enabled!";

const char kContextualSearchRankerQueryName[] =
    "Contextual Search Ranker prediction";
const char kContextualSearchRankerQueryDescription[] =
    "Enables prediction of tap gestures using Assist-Ranker machine learning.";

const char kContextualSearchSimplifiedServerName[] =
    "Contextual Search simplified server logic";
const char kContextualSearchSimplifiedServerDescription[] =
    "Enables simpler server-side logic for determining what data to return and "
    "show in the Contextual Search UI.  Option to allow all cards CoCa "
    "returns.";

const char kContextualSearchSecondTapName[] =
    "Contextual Search second tap triggering";
const char kContextualSearchSecondTapDescription[] =
    "Enables triggering on a second tap gesture even when Ranker would "
    "normally suppress that tap.";

const char kContextualSearchUnityIntegrationName[] =
    "Contextual Search integration with Unified Consent";
const char kContextualSearchUnityIntegrationDescription[] =
    "Enables integration of Tap to Search with Unified Consent.";

const char kDontPrefetchLibrariesName[] = "Don't Prefetch Libraries";
const char kDontPrefetchLibrariesDescription[] =
    "Don't prefetch libraries after loading.";

const char kDownloadsLocationChangeName[] = "Enable downloads location change";
const char kDownloadsLocationChangeDescription[] =
    "Enable changing default downloads storage location on Android.";

const char kDownloadProgressInfoBarName[] = "Enable download progress infobar";
const char kDownloadProgressInfoBarDescription[] =
    "Enables an infobar notifying users about status of current downloads.";

const char kDownloadHomeV2Name[] = "Enable download home v2";
const char kDownloadHomeV2Description[] =
    "Enables the new UI for download home";

const char kAutofillManualFallbackAndroidName[] =
    "Enable Autofill manual fallback for Addresses and Payments (Android)";
const char kAutofillManualFallbackAndroidDescription[] =
    "If enabled, adds toggle for addresses and payments bottom sheet to the "
    "keyboard accessory.";

const char kEnableAutofillRefreshStyleName[] =
    "Enable Autofill refresh style (Android)";
const char kEnableAutofillRefreshStyleDescription[] =
    "Enable modernized style for Autofill on Android";

const char kEnableAndroidSpellcheckerName[] = "Enable spell checking";
const char kEnableAndroidSpellcheckerDescription[] =
    "Enables use of the Android spellchecker.";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnableContentSuggestionsThumbnailDominantColorName[] =
    "Use content suggestions thumbnail dominant color.";
const char kEnableContentSuggestionsThumbnailDominantColorDescription[] =
    "Use content suggestions thumbnail dominant color as a placeholder before "
    "the real thumbnail is fetched (requires Chrome Home).";

const char kEnableCustomContextMenuName[] = "Enable custom context menu";
const char kEnableCustomContextMenuDescription[] =
    "Enables a new context menu when a link, image, or video is pressed within "
    "Chrome.";

const char kEnableRevampedContextMenuName[] =
    "Enable the revamped context menu";
const char kEnableRevampedContextMenuDescription[] =
    "Enables a revamped context menu when a link, image, or video is long "
    "pressed within Chrome.";

const char kEnableNtpAssetDownloadSuggestionsName[] =
    "Show asset downloads on the New Tab page";
const char kEnableNtpAssetDownloadSuggestionsDescription[] =
    "If enabled, the list of content suggestions on the New Tab page will "
    "contain assets (e.g. books, pictures, audio) that the user downloaded for "
    "later use.";

const char kEnableNtpBookmarkSuggestionsName[] =
    "Show recently visited bookmarks on the New Tab page";
const char kEnableNtpBookmarkSuggestionsDescription[] =
    "If enabled, the list of content suggestions on the New Tab page will "
    "contain recently visited bookmarks.";

const char kEnableNtpOfflinePageDownloadSuggestionsName[] =
    "Show offline page downloads on the New Tab page";
const char kEnableNtpOfflinePageDownloadSuggestionsDescription[] =
    "If enabled, the list of content suggestions on the New Tab page will "
    "contain pages that the user downloaded for later use.";

const char kEnableNtpRemoteSuggestionsName[] =
    "Show server-side suggestions on the New Tab page";
const char kEnableNtpRemoteSuggestionsDescription[] =
    "If enabled, the list of content suggestions on the New Tab page will "
    "contain server-side suggestions (e.g., Articles for you). Furthermore, it "
    "allows to override the source used to retrieve these server-side "
    "suggestions.";

const char kEnableNtpSnippetsVisibilityName[] =
    "Make New Tab Page Snippets more visible.";
const char kEnableNtpSnippetsVisibilityDescription[] =
    "If enabled, the NTP snippets will become more discoverable with a larger "
    "portion of the first card above the fold.";

const char kEnableNtpSuggestionsNotificationsName[] =
    "Notify about new content suggestions available at the New Tab page";
const char kEnableNtpSuggestionsNotificationsDescription[] =
    "If enabled, notifications will inform about new content suggestions on "
    "the New Tab page.";

const char kEnableOfflinePreviewsName[] = "Offline Page Previews";
const char kEnableOfflinePreviewsDescription[] =
    "Enable showing offline page previews on slow networks.";

const char kEnableOskOverscrollName[] = "Enable OSK Overscroll";
const char kEnableOskOverscrollDescription[] =
    "Enable OSK overscroll support. With this flag on, the OSK will only "
    "resize the visual viewport.";

const char kEnableWebNfcName[] = "WebNFC";
const char kEnableWebNfcDescription[] = "Enable WebNFC support.";

const char kEphemeralTabName[] = "An Ephemeral Tab in an Overlay Panel";
const char kEphemeralTabDescription[] =
    "Enable a 'Preview page/image' at a linked page into an overlay.";

const char kExploreSitesName[] = "Explore websites";
const char kExploreSitesDescription[] =
    "Enables portal from new tab page to explore websites.";

const char kForegroundNotificationManagerName[] =
    "Foreground notification manager";
const char kForegroundNotificationManagerDescription[] =
    "Enable foreground notification manager to handle foreground service and "
    "notification.";

const char kGrantNotificationsToDSEName[] =
    "Grant notifications to the Default Search Engine";
const char kGrantNotificationsToDSENameDescription[] =
    "Automatically grant the notifications permission to the Default Search "
    "Engine";

const char kHomePageButtonName[] = "Force Enable Home Page Button";
const char kHomePageButtonDescription[] = "Displays a home button if enabled.";

const char kHomepageTileName[] =
    "Enable Homepage tile shown in Suggested Tiles";
const char kHomepageTileDescription[] =
    "When NTPButton is enabled, the first tile of the Suggested Tiles will be "
    "used for homepage. It will not have an effect when NTPButton is disabled.";

const char kIncognitoStringsName[] = "Alternate incognito strings";
const char kIncognitoStringsDescription[] =
    "Show alternate incognito strings if enabled.";

const char kInterestFeedContentSuggestionsDescription[] =
    "Use the interest feed to render content suggestions. Currently "
    "content "
    "suggestions are shown on the New Tab Page.";
const char kInterestFeedContentSuggestionsName[] =
    "Interest Feed Content Suggestions";

const char kKeepPrefetchedContentSuggestionsName[] =
    "Keep prefetched content suggestions";
const char kKeepPrefetchedContentSuggestionsDescription[] =
    "If enabled, some of prefetched content suggestions are not replaced by "
    "the new fetched suggestions.";

const char kLanguagesPreferenceName[] = "Language Settings";
const char kLanguagesPreferenceDescription[] =
    "Enable this option for Language Settings feature on Android.";

const char kLsdPermissionPromptName[] =
    "Location Settings Dialog Permission Prompt";
const char kLsdPermissionPromptDescription[] =
    "Whether to use the Google Play Services Location Settings Dialog "
    "permission dialog.";

const char kManualPasswordGenerationAndroidName[] =
    "Manual password generation";
const char kManualPasswordGenerationAndroidDescription[] =
    "Whether Chrome should offer users the option to manually request to "
    "generate passwords on Android.";

const char kMediaScreenCaptureName[] = "Experimental ScreenCapture.";
const char kMediaScreenCaptureDescription[] =
    "Enable this option for experimental ScreenCapture feature on Android.";

const char kModalPermissionDialogViewName[] = "Modal Permission Dialog";
const char kModalPermissionDialogViewDescription[] =
    "Enable this option to use ModalDialogManager for permission Dialogs.";

const char kNewContactsPickerName[] = "Enable new contacts picker";
const char kNewContactsPickerDescription[] =
    "Activates the new picker for selecting contacts.";

const char kNewNetErrorPageUIName[] = "Enable new UI for net-error page";
const char kNewNetErrorPageUIDescription[] =
    "Selects which new UI experience to show on the net-error (Dino) page";

const char kNewPhotoPickerName[] = "Enable new Photopicker";
const char kNewPhotoPickerDescription[] =
    "Activates the new picker for selecting photos.";

const char kNoCreditCardAbort[] = "No Credit Card Abort";
const char kNoCreditCardAbortDescription[] =
    "Whether or not the No Credit Card Abort is enabled.";

const char kNtpButtonName[] = "Enable NTP Button";
const char kNtpButtonDescription[] =
    "Displays a New Tab Page button in the toolbar if enabled.";

const char kOfflineIndicatorAlwaysHttpProbeName[] = "Always http probe";
const char kOfflineIndicatorAlwaysHttpProbeDescription[] =
    "Always do http probe to detect network connectivity for offline indicator "
    "as opposed to just taking the connection state from the system."
    "Used for testing.";

const char kOfflineIndicatorChoiceName[] = "Offline indicator choices";
const char kOfflineIndicatorChoiceDescription[] =
    "Show an offline indicator while offline.";

const char kOfflinePagesCtName[] = "Enable Offline Pages CT features.";
const char kOfflinePagesCtDescription[] = "Enable Offline Pages CT features.";

const char kOfflinePagesCtV2Name[] = "Enable Offline Pages CT V2 features.";
const char kOfflinePagesCtV2Description[] =
    "V2 features include attributing pages to the app that initiated the "
    "custom tabs, and being able to query for pages by page attribution.";

const char kOfflinePagesCTSuppressNotificationsName[] =
    "Disable download complete notification for whitelisted CCT apps.";
const char kOfflinePagesCTSuppressNotificationsDescription[] =
    "Disable download complete notification for page downloads originating "
    "from a CCT app whitelisted to show their own download complete "
    "notification.";

const char kOfflinePagesDescriptiveFailStatusName[] =
    "Enables descriptive failed download status text.";
const char kOfflinePagesDescriptiveFailStatusDescription[] =
    "Enables failed download status text in notifications and Downloads Home "
    "to state the reason the request failed if the failure is actionable.";

const char kOfflinePagesDescriptivePendingStatusName[] =
    "Enables descriptive pending download status text.";
const char kOfflinePagesDescriptivePendingStatusDescription[] =
    "Enables pending download status text in notifications and Downloads Home "
    "to state the reason the request is pending.";

const char kOfflinePagesInDownloadHomeOpenInCctName[] =
    "Enables offline pages in the downloads home to be opened in CCT.";
const char kOfflinePagesInDownloadHomeOpenInCctDescription[] =
    "When enabled offline pages launched from the Downloads Home will be "
    "opened in Chrome Custom Tabs (CCT) instead of regular tabs.";

const char kOfflinePagesLimitlessPrefetchingName[] =
    "Removes resource usage limits for the prefetching of offline pages.";
const char kOfflinePagesLimitlessPrefetchingDescription[] =
    "Allows the prefetching of suggested offline pages to ignore resource "
    "usage limits. This allows it to completely ignore data usage limitations "
    "and allows downloads to happen with any kind of connection.";

const char kOfflinePagesLoadSignalCollectingName[] =
    "Enables collecting load timing data for offline page snapshots.";
const char kOfflinePagesLoadSignalCollectingDescription[] =
    "Enables loading completeness data collection while writing an offline "
    "page.  This data is collected in the snapshotted offline page to allow "
    "data analysis to improve deciding when to make the offline snapshot.";

const char kOfflinePagesPrefetchingName[] =
    "Enables suggested offline pages to be prefetched.";
const char kOfflinePagesPrefetchingDescription[] =
    "Enables suggested offline pages to be prefetched, so useful content is "
    "available while offline.";

const char kOfflinePagesResourceBasedSnapshotName[] =
    "Enables offline page snapshots to be based on percentage of page loaded.";
const char kOfflinePagesResourceBasedSnapshotDescription[] =
    "Enables offline page snapshots to use a resource percentage based "
    "approach for determining when the page is loaded as opposed to a time "
    "based approach";

const char kOfflinePagesRenovationsName[] = "Enables offline page renovations.";
const char kOfflinePagesRenovationsDescription[] =
    "Enables offline page renovations which correct issues with dynamic "
    "content that occur when offlining pages that use JavaScript.";

const char kOfflinePagesLivePageSharingName[] =
    "Enables live page sharing of offline pages";
const char kOfflinePagesLivePageSharingDescription[] =
    "Enables to share current loaded page as offline page by saving as MHTML "
    "first.";

const char kOfflinePagesShowAlternateDinoPageName[] =
    "Enable alternate dino page with more user capabilities.";
const char kOfflinePagesShowAlternateDinoPageDescription[] =
    "Enables the dino page to show more buttons and offer existing offline "
    "content.";

const char kOfflinePagesSvelteConcurrentLoadingName[] =
    "Enables concurrent background loading on svelte.";
const char kOfflinePagesSvelteConcurrentLoadingDescription[] =
    "Enables concurrent background loading (or downloading) of pages on "
    "Android svelte (512MB RAM) devices. Otherwise, background loading will "
    "happen when the svelte device is idle.";

const char kOffliningRecentPagesName[] =
    "Enable offlining of recently visited pages";
const char kOffliningRecentPagesDescription[] =
    "Enable storing recently visited pages locally for offline use. Requires "
    "Offline Pages to be enabled.";

const char kProgressBarThrottleName[] = "Android progress update throttling.";
const char kProgressBarThrottleDescription[] =
    "Limit the maximum progress update to make progress appear smoother.";

const char kPullToRefreshEffectName[] = "The pull-to-refresh effect";
const char kPullToRefreshEffectDescription[] =
    "Page reloads triggered by vertically overscrolling content.";

const char kPwaImprovedSplashScreenName[] =
    "Improved Splash Screen for standalone PWAs";
const char kPwaImprovedSplashScreenDescription[] =
    "Enables the Improved Splash Screen UX for standalone PWAs based on new "
    "Web App Manifest attributes";
const char kPwaPersistentNotificationName[] =
    "Persistent notification in standalone PWA";
const char kPwaPersistentNotificationDescription[] =
    "Enables a persistent Android notification for standalone PWAs";

const char kReaderModeHeuristicsName[] = "Reader Mode triggering";
const char kReaderModeHeuristicsDescription[] =
    "Determines what pages the Reader Mode infobar is shown on.";
const char kReaderModeHeuristicsMarkup[] = "With article structured markup";
const char kReaderModeHeuristicsAdaboost[] = "Non-mobile-friendly articles";
const char kReaderModeHeuristicsAllArticles[] = "All articles";
const char kReaderModeHeuristicsAlwaysOff[] = "Never";
const char kReaderModeHeuristicsAlwaysOn[] = "Always";

const char kReaderModeInCCTName[] = "Reader Mode in CCT";
const char kReaderModeInCCTDescription[] =
    "Open Reader Mode in Chrome Custom Tabs.";

const char kSafeBrowsingTelemetryForApkDownloadsName[] =
    "Send some telemetry for APK downloads for extended reporting users";
const char kSafeBrowsingTelemetryForApkDownloadsDescription[] =
    "If enabled, sends some information about the source and hash of the "
    "contents of any APK files downloaded by a user who has opted into Safe "
    "Browsing Extended Reporting already.";

const char kSafeBrowsingUseLocalBlacklistsV2Name[] =
    "Use local Safe Browsing blacklists";
const char kSafeBrowsingUseLocalBlacklistsV2Description[] =
    "If enabled, maintain a copy of Safe Browsing blacklists in the browser "
    "process to check the Safe Browsing reputation of URLs without calling "
    "into GmsCore for every URL.";

const char kSearchReadyOmniboxName[] = "Search Ready Omnibox";
const char kSearchReadyOmniboxDescription[] =
    "Clears the omnibox and adds a suggestion item to share, copy, or edit the "
    "URL.";

const char kSetMarketUrlForTestingName[] = "Set market URL for testing";
const char kSetMarketUrlForTestingDescription[] =
    "When enabled, sets the market URL for use in testing the update menu "
    "item.";

const char kSiteExplorationUiName[] = "Site Exploration UI";
const char kSiteExplorationUiDescription[] =
    "Show site suggestions in the Exploration UI";

const char kSiteIsolationForPasswordSitesName[] =
    "Site Isolation For Password Sites";
const char kSiteIsolationForPasswordSitesDescription[] =
    "Security mode that enables site isolation for sites based on "
    "password-oriented heuristics, such as a user typing in a password.";

const char kStrictSiteIsolationName[] = "Strict site isolation";
const char kStrictSiteIsolationDescription[] =
    "Security mode that enables site isolation for all sites (SitePerProcess). "
    "In this mode, each renderer process will contain pages from at most one "
    "site, using out-of-process iframes when needed. "
    "Check chrome://process-internals to see the current isolation mode. "
    "Setting this flag to 'Enabled' turns on site isolation regardless of the "
    "default. Here, 'Disabled' is a legacy value that actually means "
    "'Default,' in which case site isolation may be already enabled based on "
    "platform, enterprise policy, or field trial. See also "
    "#site-isolation-trial-opt-out for how to disable site isolation for "
    "testing.";

const char kTranslateAndroidManualTriggerName[] =
    "Enable manual translate trigger";
const char kTranslateAndroidManualTriggerDescription[] =
    "Show a menu item in the main menu that triggers page translation.";

const char kUpdateMenuBadgeName[] = "Force show update menu badge";
const char kUpdateMenuBadgeDescription[] =
    "When enabled, a badge will be shown on the app menu button if the update "
    "type is Update Available or Unsupported OS Version.";

const char kUpdateMenuItemCustomSummaryDescription[] =
    "When this flag and the force show update menu item flag are enabled, a "
    "custom summary string will be displayed below the update menu item.";
const char kUpdateMenuItemCustomSummaryName[] =
    "Update menu item custom summary";

const char kUpdateMenuTypeName[] =
    "Forces the update menu type to a specific type";
const char kUpdateMenuTypeDescription[] =
    "When set, forces the update type to be a specific one, which impacts "
    "the app menu badge and menu item for updates. For Inline Update, the "
    "update available flag is implied. The 'Inline Update: Success' selection "
    "goes through the whole inline update flow to the end with a successful "
    "outcome. The other 'Inline Update' options go through the same flow, but "
    "stop at various stages, see their error type for details.";
const char kUpdateMenuTypeNone[] = "None";
const char kUpdateMenuTypeUpdateAvailable[] = "Update Available";
const char kUpdateMenuTypeUnsupportedOSVersion[] = "Unsupported OS Version";
const char kUpdateMenuTypeInlineUpdateSuccess[] = "Inline Update: Success";
const char kUpdateMenuTypeInlineUpdateDialogCanceled[] =
    "Inline Update Error: Dialog Canceled";
const char kUpdateMenuTypeInlineUpdateDialogFailed[] =
    "Inline Update Error: Dialog Failed";
const char kUpdateMenuTypeInlineUpdateDownloadFailed[] =
    "Inline Update Error: Download Failed";
const char kUpdateMenuTypeInlineUpdateDownloadCanceled[] =
    "Inline Update Error: Download Canceled";
const char kUpdateMenuTypeInlineUpdateInstallFailed[] =
    "Inline Update Error: Install Failed";

const char kInlineUpdateFlowName[] = "Enable Google Play inline update flow";
const char kInlineUpdateFlowDescription[] =
    "When this flag is set, instead of taking the user to the Google Play "
    "Store when an update is available, the user is presented with an inline "
    "flow where they do not have to leave Chrome until the update is ready "
    "to install.";

#if BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

const char kAndroidNightModeName[] = "Android Chrome UI dark mode";
const char kAndroidNightModeDescription[] =
    "If enabled, user can enable Android Chrome UI dark mode through settings.";

#endif  // BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

// Non-Android -----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

const char kAccountConsistencyName[] =
    "Identity consistency between browser and cookie jar";
const char kAccountConsistencyDescription[] =
    "When enabled, the browser manages signing in and out of Google accounts.";
const char kAccountConsistencyChoiceMirror[] = "Mirror";
const char kAccountConsistencyChoiceDice[] = "Dice";

const char kAppManagementName[] = "Enable App Management page";
const char kAppManagementDescription[] =
    "Shows the new app management page at chrome://apps.";

const char kAutofillDropdownLayoutName[] =
    "Autofill Dropdown Layout Experiment";
const char kAutofillDropdownLayoutDescription[] =
    "Alternate visual designs for the Autofill dropdown.";

const char kDoodlesOnLocalNtpName[] = "Enable doodles on the local NTP";
const char kDoodlesOnLocalNtpDescription[] =
    "Show doodles on the local New Tab page if Google is the default search "
    "engine.";

const char kSearchSuggestionsOnLocalNtpName[] =
    "Enable search suggestions on the local NTP";
const char kSearchSuggestionsOnLocalNtpDescription[] =
    "Show search suggestions on the local New Tab page if Google is the "
    "default search engine.";

const char kPromosOnLocalNtpName[] = "Enable promos on the local NTP";
const char kPromosOnLocalNtpDescription[] =
    "Show promos on the local New Tab page if Google is the "
    "default search engine.";

const char kRemoveNtpFakeboxName[] = "Remove fakebox from the NTP";
const char kRemoveNtpFakeboxDescription[] =
    "Do not show the fakebox on the New Tab page.";

const char kEnableWebAuthenticationBleSupportName[] =
    "Web Authentication API BLE support";
const char kEnableWebAuthenticationBleSupportDescription[] =
    "Enable support for using Web Authentication API via Bluetooth security "
    "keys";

const char kEnableWebAuthenticationTestingAPIName[] =
    "Web Authentication Testing API";
const char kEnableWebAuthenticationTestingAPIDescription[] =
    "Enable Web Authentication Testing API support, which disconnects the API "
    "implementation from the real world, and allows configuring virtual "
    "authenticator devices for testing";

const char kHappinessTrackingSurveysForDesktopName[] =
    "Happiness Tracking Surveys";
const char kHappinessTrackingSurveysForDesktopDescription[] =
    "Enable showing Happiness Tracking Surveys to users on Desktop";

const char kLinkManagedNoticeToChromeUIManagementURLName[] =
    "Link managed notice to the management page";
const char kLinkManagedNoticeToChromeUIManagementURLDescription[] =
    "Makes the managed notice 'Managed by your organization' a link to "
    "chrome://management";

const char kOmniboxDriveSuggestionsName[] =
    "Omnibox Google Drive Document suggestions";
const char kOmniboxDriveSuggestionsDescriptions[] =
    "Display suggestions for Google Drive documents in the omnibox when Google "
    "is the default search engine.";

const char kOmniboxDeduplicateDriveUrlsName[] =
    "Deduplicate Google Drive suggestions in the Omnibox";
const char kOmniboxDeduplicateDriveUrlsDescription[] =
    "Present at most one result for the same Drive document across bookmarks, "
    "history, document, etc. suggestions.";

const char kOmniboxExperimentalKeywordModeName[] =
    "Omnibox Experimental Keyword Mode";
const char kOmniboxExperimentalKeywordModeDescription[] =
    "Enables various experimental features related to keyword mode, its "
    "suggestions and layout";

const char kOmniboxPedalSuggestionsName[] = "Omnibox Pedal suggestions";
const char kOmniboxPedalSuggestionsDescription[] =
    "Enable omnibox Pedal suggestions to accelerate actions within Chrome by "
    "detecting user intent and offering direct access to the end goal.";

const char kOmniboxReverseAnswersName[] = "Omnibox reverse answers";
const char kOmniboxReverseAnswersDescription[] =
    "Display answers with rows reversed (swapped); except definitions. Has no "
    "effect unless either the #upcoming-ui-features flag is Enabled or the "
    "#top-chrome-md flag is set to Refresh or Touchable Refresh.";

const char kOmniboxReverseTabSwitchLogicName[] =
    "Omnibox reverse tab switch logic";
const char kOmniboxReverseTabSwitchLogicDescription[] =
    "Reverse the logic of suggestions that have a tab switch button: Have "
    "them switch by default, and have the button navigate.";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Omnibox tab switch suggestions";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable suggestions for switching to open tabs within the Omnibox. "
    "Has no effect unless either the #upcoming-ui-features flag is Enabled or "
    "the #top-chrome-md flag is set to Refresh or Touchable Refresh.";

const char kOmniboxTailSuggestionsName[] = "Omnibox tail suggestions";
const char kOmniboxTailSuggestionsDescription[] =
    "Enable receiving tail suggestions, a type of search suggestion based on "
    "the last few words in the query, for the Omnibox.";

const char kPageAlmostIdleName[] = "Page Almost Idle";
const char kPageAlmostIdleDescription[] =
    "Make session restore use a definition of loading that waits for CPU and "
    "network quiescence.";

const char kProactiveTabFreezeAndDiscardName[] =
    "Proactive Tab Freeze and Discard";
const char kProactiveTabFreezeAndDiscardDescription[] =
    "Enables proactive tab freezing and discarding. This requires "
    "#enable-page-almost-idle.";

const char kShowManagedUiName[] = "Show managed UI for managed users";
const char kShowManagedUiDescription[] =
    "Enabled/disable showing enterprise users a 'Managed by your organization' "
    "message in the app menu and on some chrome:// pages.";

const char kSiteCharacteristicsDatabaseName[] = "Site Characteristics database";
const char kSiteCharacteristicsDatabaseDescription[] =
    "Records usage of some features in a database while a tab is in background "
    "(title/favicon update, audio playback or usage of non-persistent "
    "notifications).";

const char kUseGoogleLocalNtpName[] = "Enable using the Google local NTP";
const char kUseGoogleLocalNtpDescription[] =
    "Use the local New Tab page if Google is the default search engine.";

#if defined(GOOGLE_CHROME_BUILD)

const char kGoogleBrandedContextMenuName[] =
    "Google branding in the context menu";
const char kGoogleBrandedContextMenuDescription[] =
    "Shows a Google icon next to context menu items powered by Google "
    "services.";

#endif  // !defined(GOOGLE_CHROME_BUILD)

#endif  // !defined(OS_ANDROID)

// Windows ---------------------------------------------------------------------

#if defined(OS_WIN)

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kCloudPrintXpsName[] = "XPS in Google Cloud Print";
const char kCloudPrintXpsDescription[] =
    "XPS enables advanced options for classic printers connected to the Cloud "
    "Print with Chrome. Printers must be re-connected after changing this "
    "flag.";

const char kD3D11VideoDecoderName[] = "D3D11 Video Decoder";
const char kD3D11VideoDecoderDescription[] =
    "Enables D3D11VideoDecoder for hardware accelerated video decoding.";

const char kDisablePostscriptPrinting[] = "Disable PostScript Printing";
const char kDisablePostscriptPrintingDescription[] =
    "Disables PostScript generation when printing to PostScript capable "
    "printers, and uses EMF generation in its place.";

const char kEnableAppcontainerName[] = "Enable AppContainer Lockdown.";
const char kEnableAppcontainerDescription[] =
    "Enables the use of an AppContainer on sandboxed processes to improve "
    "security.";

const char kEnableGpuAppcontainerName[] = "Enable GPU AppContainer Lockdown.";
const char kEnableGpuAppcontainerDescription[] =
    "Enables the use of an AppContainer for the GPU sandboxed processes to "
    "improve security.";

const char kGdiTextPrinting[] = "GDI Text Printing";
const char kGdiTextPrintingDescription[] =
    "Use GDI to print text as simply text";

const char kUseAngleName[] = "Choose ANGLE graphics backend";
const char kUseAngleDescription[] =
    "Choose the graphics backend for ANGLE. D3D11 is used on most Windows "
    "computers by default. Using the OpenGL driver as the graphics backend may "
    "result in higher performance in some graphics-heavy applications, "
    "particularly on NVIDIA GPUs. It can increase battery and memory usage of "
    "video playback.";

const char kUseAngleDefault[] = "Default";
const char kUseAngleGL[] = "OpenGL";
const char kUseAngleD3D11[] = "D3D11";
const char kUseAngleD3D9[] = "D3D9";

const char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
const char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

#endif  // defined(OS_WIN)

// Mac -------------------------------------------------------------------------

#if defined(OS_MACOSX)

const char kContentFullscreenName[] = "Improved Content Fullscreen";
const char kContentFullscreenDescription[] =
    "Fullscreen content window detaches from main browser window and goes to "
    "a new space without moving or changing the original browser window.";

const char kHostedAppsInWindowsName[] =
    "Allow hosted apps to be opened in windows";
const char kHostedAppsInWindowsDescription[] =
    "Allows hosted apps to be opened in windows instead of being limited to "
    "tabs.";

const char kCreateAppWindowsInAppShimProcessName[] =
    "Create native windows in the app process";
const char kCreateAppWindowsInAppShimProcessDescription[] =
    "Create native windows the app shim process, instead of of the browser "
    "process.";

const char kEnableCustomMacPaperSizesName[] = "Enable custom paper sizes";
const char kEnableCustomMacPaperSizesDescription[] =
    "Allow use of custom paper sizes in Print Preview.";

const char kMacTouchBarName[] = "Hardware Touch Bar";
const char kMacTouchBarDescription[] = "Control the use of the Touch Bar.";

const char kMacV2GPUSandboxName[] = "Mac V2 GPU Sandbox";
const char kMacV2GPUSandboxDescription[] =
    "Controls whether the GPU process on macOS uses the V1 or V2 sandbox.";

const char kMacViewsNativeAppWindowsName[] = "Toolkit-Views App Windows.";
const char kMacViewsNativeAppWindowsDescription[] =
    "Controls whether to use Toolkit-Views based Chrome App windows.";

const char kMacViewsTaskManagerName[] = "Toolkit-Views Task Manager.";
const char kMacViewsTaskManagerDescription[] =
    "Controls whether to use the Toolkit-Views based Task Manager.";

#endif

// Chrome OS -------------------------------------------------------------------

#if defined(OS_CHROMEOS)

const char kAcceleratedMjpegDecodeName[] =
    "Hardware-accelerated mjpeg decode for captured frame";
const char kAcceleratedMjpegDecodeDescription[] =
    "Enable hardware-accelerated mjpeg decode for captured frame where "
    "available.";

const char kAllowTouchpadThreeFingerClickName[] = "Touchpad three-finger-click";
const char kAllowTouchpadThreeFingerClickDescription[] =
    "Enables touchpad three-finger-click as middle button.";

const char kAppServiceAshName[] = "App Service Ash";
const char kAppServiceAshDescription[] =
    "Use the App Service to provide data to the Ash UI, such as the shelf and "
    "app list.";

const char kArcAvailableForChildName[] = "Allow ARC for child accounts";
const char kArcAvailableForChildDescription[] =
    "Allow child accounts to start Android apps.";

const char kArcBootCompleted[] = "Load Android apps automatically";
const char kArcBootCompletedDescription[] =
    "Allow Android apps to start automatically after signing in.";

const char kArcCupsApiName[] = "ARC CUPS API";
const char kArcCupsApiDescription[] =
    "Enables support of libcups APIs from ARC";

const char kArcDocumentsProviderName[] = "ARC DocumentsProvider integration";
const char kArcDocumentsProviderDescription[] =
    "Enables DocumentsProvider integration in Chrome OS Files app.";

const char kArcFilePickerExperimentName[] =
    "Enable file picker experiment for ARC";
const char kArcFilePickerExperimentDescription[] =
    "Enables using Chrome OS file picker in ARC.";

const char kArcGraphicBuffersVisualizationToolName[] =
    "Enable ARC graphic buffers visualization tool";
const char kArcGraphicBuffersVisualizationToolDescription[] =
    "Enable ARC graphic buffers visualization tool "
    "(chrome://arc-graphics-tracing).";

const char kArcNativeBridgeExperimentName[] =
    "Enable native bridge experiment for ARC";
const char kArcNativeBridgeExperimentDescription[] =
    "Enables experimental native bridge feature.";

const char kArcUsbHostName[] = "Enable ARC USB host integration";
const char kArcUsbHostDescription[] =
    "Allow Android apps to use USB host feature on ChromeOS devices.";

const char kArcVpnName[] = "Enable ARC VPN integration";
const char kArcVpnDescription[] =
    "Allow Android VPN clients to tunnel Chrome traffic.";

const char kAshEnableDisplayMoveWindowAccelsName[] =
    "Enable shortcuts for moving window between displays.";
const char kAshEnableDisplayMoveWindowAccelsDescription[] =
    "Enable shortcuts for moving window between displays.";

const char kAshEnableOverviewRoundedCornersName[] =
    "Enable rounded corners in overview mode.";
const char kAshEnableOverviewRoundedCornersDescription[] =
    "Enables rounded corners on overview windows.";

const char kAshEnablePersistentWindowBoundsName[] =
    "Enable persistent window bounds in multi-displays scenario.";
const char kAshEnablePersistentWindowBoundsDescription[] =
    "Enable persistent window bounds in multi-displays scenario.";

const char kAshEnablePipRoundedCornersName[] =
    "Enable Picture-in-Picture rounded corners.";
const char kAshEnablePipRoundedCornersDescription[] =
    "Enable rounded corners on the Picture-in-Picture window.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

const char kAshShelfColorName[] = "Shelf color in Chrome OS system UI";
const char kAshShelfColorDescription[] =
    "Enables/disables the shelf color to be a derived from the wallpaper. The "
    "--ash-shelf-color-scheme flag defines how that color is derived.";

const char kAshShelfColorScheme[] = "Shelf color scheme in Chrome OS System UI";
const char kAshShelfColorSchemeDescription[] =
    "Specify how the color is derived from the wallpaper. This flag is only "
    "used when the --ash-shelf-color flag is enabled. Defaults to Dark & Muted";
const char kAshShelfColorSchemeLightVibrant[] = "Light & Vibrant";
const char kAshShelfColorSchemeNormalVibrant[] = "Normal & Vibrant";
const char kAshShelfColorSchemeDarkVibrant[] = "Dark & Vibrant";
const char kAshShelfColorSchemeLightMuted[] = "Light & Muted";
const char kAshShelfColorSchemeNormalMuted[] = "Normal & Muted";
const char kAshShelfColorSchemeDarkMuted[] = "Dark & Muted";

const char kBulkPrintersName[] = "Bulk Printers Policy";
const char kBulkPrintersDescription[] = "Enables the new bulk printers policy";

const char kCaptivePortalBypassProxyName[] =
    "Bypass proxy for Captive Portal Authorization";
const char kCaptivePortalBypassProxyDescription[] =
    "If proxy is configured, it usually prevents from authorization on "
    "different captive portals. This enables opening captive portal "
    "authorization dialog in a separate window, which ignores proxy settings.";

const char kCrOSContainerName[] = "Chrome OS Container";
const char kCrOSContainerDescription[] =
    "Enable the use of Chrome OS Container utility.";

const char kCrosRegionsModeName[] = "Cros-regions load mode";
const char kCrosRegionsModeDescription[] =
    "This flag controls cros-regions load mode";
const char kCrosRegionsModeDefault[] = "Default";
const char kCrosRegionsModeOverride[] = "Override VPD values.";
const char kCrosRegionsModeHide[] = "Hide VPD values.";

const char kCrostiniAppSearchName[] = "Crostini App Search";
const char kCrostiniAppSearchDescription[] =
    "Enable search and installation of Crostini apps in the launcher.";

const char kCrostiniBackupName[] = "Crostini Backup";
const char kCrostiniBackupDescription[] = "Enable Crostini export and import.";

const char kCrostiniUsbSupportName[] = "Crostini Usb Support";
const char kCrostiniUsbSupportDescription[] =
    "Enable mounting Usb devices in Crostini.";

const char kCryptAuthV2EnrollmentName[] = "CryptAuth v2 Enrollment";
const char kCryptAuthV2EnrollmentDescription[] =
    "Use the CryptAuth v2 Enrollment protocol.";

const char kDisableExplicitDmaFencesName[] = "Disable explicit dma-fences";
const char kDisableExplicitDmaFencesDescription[] =
    "Always rely on implicit syncrhonization between GPU and display "
    "controller instead of using dma-fences explcitily when available.";

const char kDisableSystemTimezoneAutomaticDetectionName[] =
    "SystemTimezoneAutomaticDetection policy support";
const char kDisableSystemTimezoneAutomaticDetectionDescription[] =
    "Disable system timezone automatic detection device policy.";

const char kDisableTabletAutohideTitlebarsName[] =
    "Disable autohide titlebars in tablet mode";
const char kDisableTabletAutohideTitlebarsDescription[] =
    "Disable tablet mode autohide titlebars functionality. The user will be "
    "able to see the titlebar in tablet mode.";

const char kDisableTabletSplitViewName[] = "Disable split view in Tablet mode";
const char kDisableTabletSplitViewDescription[] =
    "Disable split view for Chrome OS tablet mode.";

const char kDoubleTapToZoomInTabletModeName[] =
    "Double-tap to zoom in tablet mode";
const char kDoubleTapToZoomInTabletModeDescription[] =
    "If Enabled, double tapping in webpages while in tablet mode will zoom the "
    "page.";

const char kEnableAppGridGhostName[] = "App Grid Ghosting";
const char kEnableAppGridGhostDescription[] =
    "Enables ghosting during an item drag in launcher.";

const char kEnableAppListSearchAutocompleteName[] =
    "App List Search Autocomplete";
const char kEnableAppListSearchAutocompleteDescription[] =
    "Allow App List search box to autocomplete queries for Google searches and "
    "apps.";

const char kEnableAppShortcutSearchName[] =
    "Enable app shortcut search in launcher";
const char kEnableAppShortcutSearchDescription[] =
    "Enables app shortcut search in launcher";

const char kEnableAppDataSearchName[] = "Enable app data search in launcher";
const char kEnableAppDataSearchDescription[] =
    "Allow launcher search to access data available through Firebase App "
    "Indexing";

const char kEnableArcUnifiedAudioFocusName[] =
    "Enable unified audio focus on ARC";
const char kEnableArcUnifiedAudioFocusDescription[] =
    "If audio focus is enabled in Chrome then this will delegate audio focus "
    "control in Android apps to Chrome.";

const char kEnableAssistantVoiceMatchName[] = "Enable Assistant Voice Match";
const char kEnableAssistantVoiceMatchDescription[] =
    "Enable the Assistant Voice Match feature";

const char kEnableAssistantAppSupportName[] = "Enable Assistant App Support";
const char kEnableAssistantAppSupportDescription[] =
    "Enable the Assistant App Support feature";

const char kEnableChromeOsAccountManagerName[] = "Enable Account Manager";
const char kEnableChromeOsAccountManagerDescription[] =
    "Enables the Chrome OS Account Manager";

const char kEnableDiscoverAppName[] = "Enable Discover App";
const char kEnableDiscoverAppDescription[] =
    "Enable Discover App icon in launcher.";

const char kEnableDriveFsName[] = "Enable DriveFS";
const char kEnableDriveFsDescription[] =
    "Enables use of the new DriveFS-based Drive sync client.";

const char kEnableEncryptionMigrationName[] =
    "Enable encryption migration of user data";
const char kEnableEncryptionMigrationDescription[] =
    "If enabled and the device supports ARC, the user will be asked to update "
    "the encryption of user data when the user signs in.";

const char kEnableFullscreenHandwritingVirtualKeyboardName[] =
    "Enable full screen handwriting virtual keyboard";
const char kEnableFullscreenHandwritingVirtualKeyboardDescription[] =
    "If enabled, the handwriting virtual keyboard will allow user to write "
    "anywhere on the screen";

const char kEnableGoogleAssistantName[] = "Enable Google Assistant";
const char kEnableGoogleAssistantDescription[] =
    "Enable an experimental Assistant implementation that will work on all "
    "Chromebooks.";

const char kEnableGoogleAssistantDspName[] =
    "Enable Google Assistant with hardware-based hotword";
const char kEnableGoogleAssistantDspDescription[] =
    "Enable an experimental feature that uses hardware-based hotword detection "
    "for Assistant. Only a limited number of devices have this type of "
    "hardware support.";

const char kEnableGoogleAssistantStereoInputName[] =
    "Enable Google Assistant with stereo audio input";
const char kEnableGoogleAssistantStereoInputDescription[] =
    "Enable an experimental feature that uses stereo audio input for hotword "
    "and voice to text detection in Google Assistant.";

const char kEnableHomeLauncherName[] = "Enable home launcher";
const char kEnableHomeLauncherDescription[] =
    "Enable home launcher in tablet mode.";

const char kEnableMyFilesVolumeName[] = "Enable MyFiles as Volume";
const char kEnableMyFilesVolumeDescription[] =
    "Enables use of MyFiles as a read/write volume. This should be only "
    "used for testing or for trying to restore the previous Downloads content.";

const char kEnableOobeRecommendAppsScreenName[] =
    "Enable OOBE Recommend Apps Screen";
const char kEnableOobeRecommendAppsScreenDescription[] =
    "Enable the Recommend Apps Screen in OOBE which allows user to install apps"
    "from other devices";

const char kEnablePerUserTimezoneName[] = "Per-user time zone preferences.";
const char kEnablePerUserTimezoneDescription[] =
    "Chrome OS system timezone preference is stored and handled for each user "
    "individually.";

const char kEnablePlayStoreSearchName[] = "Enable Play Store search";
const char kEnablePlayStoreSearchDescription[] =
    "Enable Play Store search in launcher.";

const char kEnableStylusVirtualKeyboardName[] =
    "Enable stylus virtual keyboard";
const char kEnableStylusVirtualKeyboardDescription[] =
    "If enabled, tapping with a stylus will show the handwriting virtual "
    "keyboard.";

const char kEnableVideoPlayerNativeControlsName[] =
    "Enable native controls in video player app";
const char kEnableVideoPlayerNativeControlsDescription[] =
    "Enable native controls in video player app";

const char kEnableVirtualKeyboardUkmName[] = "Enable UKM for virtual keyboard";
const char kEnableVirtualKeyboardUkmDescription[] =
    "Enables UKM for virtual keyboard";

const char kEnableZeroStateSuggestionsName[] = "Enable Zero State Suggetions";
const char kEnableZeroStateSuggestionsDescription[] =
    "Enable Zero State Suggestions feature in Launcher, which will show "
    "suggestions when launcher search box is active with an empty query";

const char kEolNotificationName[] = "Disable Device End of Life notification.";
const char kEolNotificationDescription[] =
    "Disable Notifcation when Device is End of Life.";

const char kExperimentalAccessibilityChromeVoxLanguageSwitchingName[] =
    "Enable experimental ChromeVox language switching.";
const char kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription[] =
    "Enable ChromeVox language switching, which changes ChromeVox's "
    "output language upon detection of new language.";

const char kFirstRunUiTransitionsName[] =
    "Animated transitions in the first-run tutorial";
const char kFirstRunUiTransitionsDescription[] =
    "Transitions during first-run tutorial are animated.";

const char kForceEnableStylusToolsName[] = "Force enable stylus features";
const char kForceEnableStylusToolsDescription[] =
    "Forces display of the stylus tools menu in the shelf and the stylus "
    "section in settings, even if there is no attached stylus device.";

const char kFsNosymfollowName[] =
    "Prevent symlink traversal on user-supplied filesystems.";
const char kFsNosymfollowDescription[] =
    "Causes user-supplied filesystems to be mounted with the 'nosymfollow'"
    " option, so the chromuimos LSM denies symlink traversal on the"
    " filesystem.";

const char kGestureTypingName[] = "Gesture typing for the virtual keyboard.";
const char kGestureTypingDescription[] =
    "Enable/Disable gesture typing option in the settings page for the virtual "
    "keyboard.";

const char kImeServiceName[] = "Enable IME service";
const char kImeServiceDescription[] =
    "Enable IME service to provide the IME functionality instead of NaCl";

const char kListAllDisplayModesName[] = "List all display modes";
const char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

const char kLockScreenNotificationName[] = "Lock screen notification";
const char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

const char kMashOopVizName[] = "Out-of-process viz for mash";
const char kMashOopVizDescription[] =
    "Runs viz in a separate process when mash is enabled (otherwise viz would "
    "run in ash process)";

const char kMaterialDesignInkDropAnimationSpeedName[] =
    "Material design ink drop animation speed";
const char kMaterialDesignInkDropAnimationSpeedDescription[] =
    "Sets the speed of the experimental visual feedback animations for "
    "material design.";
const char kMaterialDesignInkDropAnimationFast[] = "Fast";
const char kMaterialDesignInkDropAnimationSlow[] = "Slow";

const char kMemoryPressureThresholdName[] =
    "Memory discard strategy for advanced pressure handling";
const char kMemoryPressureThresholdDescription[] =
    "Memory discarding strategy to use";
const char kConservativeThresholds[] =
    "Conservative memory pressure release strategy";
const char kAggressiveCacheDiscardThresholds[] =
    "Aggressive cache release strategy";
const char kAggressiveTabDiscardThresholds[] =
    "Aggressive tab release strategy";
const char kAggressiveThresholds[] =
    "Aggressive tab and cache release strategy";

const char kMtpWriteSupportName[] = "MTP write support";
const char kMtpWriteSupportDescription[] =
    "MTP write support in File System API (and file manager). In-place editing "
    "operations are not supported.";

const char kNetworkPortalNotificationName[] =
    "Notifications about captive portals";
const char kNetworkPortalNotificationDescription[] =
    "If enabled, notification is displayed when device is connected to a "
    "network behind captive portal.";

const char kNewZipUnpackerName[] = "ZIP Archiver (unpacking)";
const char kNewZipUnpackerDescription[] =
    "Use the ZIP Archiver for mounting/unpacking ZIP files";

const char kOfficeEditingComponentAppName[] =
    "Office Editing for Docs, Sheets & Slides";
const char kOfficeEditingComponentAppDescription[] =
    "Office Editing for Docs, Sheets & Slides for testing purposes.";

const char kPhysicalKeyboardAutocorrectName[] = "Physical keyboard autocorrect";
const char kPhysicalKeyboardAutocorrectDescription[] =
    "Enable physical keyboard autocorrect for US keyboard, which can provide "
    "suggestions as typing on physical keyboard.";

const char kPrinterProviderSearchAppName[] =
    "Chrome Web Store Gallery app for printer drivers";
const char kPrinterProviderSearchAppDescription[] =
    "Enables Chrome Web Store Gallery app for printer drivers. The app "
    "searches Chrome Web Store for extensions that support printing to a USB "
    "printer with specific USB ID.";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] = "Conservative";
const char kSchedulerConfigurationPerformance[] = "Performance";

const char kShillSandboxingName[] =
    "Run shill, the ChromeOS network manager, in a sandbox.";
const char kShillSandboxingDescription[] =
    "Causes shill to be run as user/group 'shill', instead of 'root'.";

const char kShowTapsName[] = "Show taps";
const char kShowTapsDescription[] =
    "Draws a circle at each touch point, which makes touch points more obvious "
    "when projecting or mirroring the display. Similar to the Android OS "
    "developer option.";

const char kShowTouchHudName[] = "Show HUD for touch points";
const char kShowTouchHudDescription[] =
    "Shows a trail of colored dots for the last few touch points. Pressing "
    "Ctrl-Alt-I shows a heads-up display view in the top-left corner. Helps "
    "debug hardware issues that generate spurious touch events.";

const char kSingleProcessMashName[] =
    "In-process window service (SingleProcessMash)";
const char kSingleProcessMashDescription[] =
    "Runs the system UI (ash) as a mojo service, but inside the browser "
    "process. The browser uses the mojo window service (ws) APIs.";

const char kSmartTextSelectionName[] = "Smart Text Selection";
const char kSmartTextSelectionDescription[] =
    "Shows quick actions for text "
    "selections in the context menu.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

const char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
const char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

// Force UI Mode
const char kUiModeName[] = "Force Ui Mode";
const char kUiModeDescription[] =
    "This flag can be used to force a certain mode on to a chromebook, "
    "despite its current orientation. \"Tablet\" means that the "
    "chromebook will act as if it were in tablet mode. \"Clamshell\" "
    "means that the chromebook will act as if it were in clamshell "
    "mode. \"Auto\" means that the chromebook will alternate between "
    "the two, based on its orientation.";
const char kUiModeTablet[] = "Tablet";
const char kUiModeClamshell[] = "Clamshell";
const char kUiModeAuto[] = "Auto (default)";

const char kUiShowCompositedLayerBordersName[] =
    "Show UI composited layer borders";
const char kUiShowCompositedLayerBordersDescription[] =
    "Show border around composited layers created by UI.";
const char kUiShowCompositedLayerBordersRenderPass[] = "RenderPass";
const char kUiShowCompositedLayerBordersSurface[] = "Surface";
const char kUiShowCompositedLayerBordersLayer[] = "Layer";
const char kUiShowCompositedLayerBordersAll[] = "All";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kUnfilteredBluetoothDevicesName[] = "Unfiltered Bluetooth devices";
const char kUnfilteredBluetoothDevicesDescription[] =
    "Shows all Bluetooth devices in UI (System Tray/Settings Page.)";

const char kUsbguardName[] = "Block new USB devices at the lock screen.";
const char kUsbguardDescription[] =
    "Prevents newly connected USB devices from operating at the lock screen"
    " until Chrome OS is unlocked to protect against malicious USB devices."
    " Already connected USB devices will continue to function.";

const char kUseMashName[] = "Out-of-process system UI (mash)";
const char kUseMashDescription[] =
    "Runs the mojo UI service (mus) and the ash window manager and system UI "
    "in a separate process.";

// TODO(mcasas): remove after https://crbug.com/771345.
const char kUseMonitorColorSpaceName[] = "Use monitor color space";
const char kUseMonitorColorSpaceDescription[] =
    "Enables Chrome to use the  color space information provided by the monitor"
    " instead of the default sRGB color space.";

const char kUserActivityPredictionMlServiceName[] =
    "ML Service Smart Dim model";
const char kUserActivityPredictionMlServiceDescription[] =
    "Uses the new ML Service model for user activity prediction (Smart Dim).";

const char kVaapiJpegImageDecodeAccelerationName[] =
    "VA-API JPEG decode acceleration for images";
const char kVaapiJpegImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of JPEG images (as opposed to camera"
    " captures) using the VA-API.";

const char kVideoPlayerChromecastSupportName[] =
    "Experimental Chromecast support for Video Player";
const char kVideoPlayerChromecastSupportDescription[] =
    "This option enables experimental Chromecast support for Video Player app "
    "on ChromeOS.";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] = "Enable virtual keyboard support.";

const char kVirtualKeyboardOverscrollName[] = "Virtual Keyboard Overscroll";
const char kVirtualKeyboardOverscrollDescription[] =
    "Enables virtual keyboard overscroll support.";

const char kWakeOnPacketsName[] = "Wake On Packets";
const char kWakeOnPacketsDescription[] =
    "Enables waking the device based on the receipt of some network packets.";

const char kEnableAppReinstallZeroStateName[] =
    "Enable Zero State App Reinstall Suggestions.";
const char kEnableAppReinstallZeroStateDescription[] =
    "Enable Zero State App Reinstall Suggestions feature in launcher, which "
    "will show app reinstall recommendations at end of zero state list.";

extern const char kAshNotificationStackingBarRedesignName[] =
    "Redesigned notification stacking bar";
extern const char kAshNotificationStackingBarRedesignDescription[] =
    "Enables the redesigned notification stacking bar UI with a \"Clear all\" "
    "button.";

#endif  // defined(OS_CHROMEOS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX)

const char kEnableInputImeApiName[] = "Enable Input IME API";
const char kEnableInputImeApiDescription[] =
    "Enable the use of chrome.input.ime API.";

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

const char kWebGL2ComputeContextName[] = "WebGL 2.0 Compute";
const char kWebGL2ComputeContextDescription[] =
    "Enable the use of WebGL 2.0 Compute API.";

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX)

const char kAutomaticTabDiscardingName[] = "Automatic tab discarding";
const char kAutomaticTabDiscardingDescription[] =
    "If enabled, tabs get automatically discarded from memory when the system "
    "memory is low. Discarded tabs are still visible on the tab strip and get "
    "reloaded when clicked on. Info about discarded tabs can be found at "
    "chrome://discards.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

const char kDirectManipulationStylusName[] = "Direct Manipulation Stylus";
const char kDirectManipulationStylusDescription[] =
    "If enabled, Chrome will scroll web pages on stylus drag.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kForceEnableSystemAecName[] = "Force enable system AEC";
const char kForceEnableSystemAecDescription[] =
    "Use system echo canceller instead of WebRTC echo canceller. If there is "
    "no system echo canceller available, getUserMedia with echo cancellation "
    "enabled will fail.";

#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kWebContentsOcclusionName[] = "Enable occlusion of web contents";
const char kWebContentsOcclusionDescription[] =
    "If enabled, web contents will behave as hidden when it is occluded by "
    "other windows.";

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kExperimentalUiName[] = "Use all upcoming UI features";
const char kExperimentalUiDescription[] = "Use all upcoming UI features.";

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_VR)

const char kWebXrOrientationSensorDeviceName[] = "Orientation sensors for XR";
const char kWebXrOrientationSensorDeviceDescription[] =
    "Allow use of orientation sensors for XR if present";

#if BUILDFLAG(ENABLE_OCULUS_VR)
const char kOculusVRName[] = "Oculus hardware support";
const char kOculusVRDescription[] =
    "If enabled, Chrome will use Oculus devices for VR (supported only on "
    "Windows 10 or later).";
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
const char kOpenVRName[] = "OpenVR hardware support";
const char kOpenVRDescription[] =
    "If enabled, Chrome will use OpenVR devices for VR (supported only on "
    "Windows 10 or later).";
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
const char kWindowsMixedRealityName[] = "Windows Mixed Reality support";
const char kWindowsMixedRealityDescription[] =
    "If enabled, Chrome will use Windows Mixed Reality devices for VR"
    " (supported only on Windows 10 or later).";
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
const char kXRSandboxName[] = "XR device sandboxing";
const char kXRSandboxDescription[] =
    "If enabled, Chrome will host VR APIs in a restricted process on desktop.";
#endif  // ENABLE_ISOLATED_XR_SERVICE

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
const char kPdfAnnotations[] = "PDF Annotations";
const char kPdfAnnotationsDescription[] = "Enable annotating PDF documents.";
#endif  // defined(OS_CHROMEOS)

const char kPdfFormSaveName[] = "Save PDF Forms";
const char kPdfFormSaveDescription[] =
    "Enable saving PDFs with filled form data.";

const char kPdfIsolationName[] = "PDF Isolation";
const char kPdfIsolationDescription[] =
    "Render PDF files from different origins in different plugin processes.";

#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

const char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)

const char kReopenTabInProductHelpName[] = "Reopen tab in-product help";
const char kReopenTabInProductHelpDescription[] =
    "Enable in-product help that guides a user to reopen a tab if it looks "
    "like they accidentally closed it.";

#endif  // BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)

#if defined(WEBRTC_USE_PIPEWIRE)

extern const char kWebrtcPipeWireCapturerName[] = "WebRTC PipeWire support";
extern const char kWebrtcPipeWireCapturerDescription[] =
    "When enabled the WebRTC will use the PipeWire multimedia server for "
    "capturing the desktop content on the Wayland display server.";

#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

const char kAvoidFlashBetweenNavigationName[] =
    "Enable flash avoidance between same-origin navigations";
const char kAvoidFlahsBetweenNavigationDescription[] =
    "Enables experimental flash avoidance when navigating between pages "
    "in the same origin. This feature is in the implementation stages and "
    "currently has no effect.";

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
