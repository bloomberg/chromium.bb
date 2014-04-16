// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in:
// ash/ash_switches.cc
// base/base_switches.cc
// chromeos/chromeos_switches.cc
// etc.
//
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// On ChromeOS, file:// access is disabled except for certain whitelisted
// directories. This switch re-enables file:// for testing.
const char kAllowFileAccess[]               = "allow-file-access";

// Allow non-secure origins to use the screen capture API and the desktopCapture
// extension API.
const char kAllowHttpScreenCapture[] = "allow-http-screen-capture";

// Specifies comma-separated list of extension ids or hosts to grant
// access to CRX file system APIs.
const char kAllowNaClCrxFsAPI[]             = "allow-nacl-crxfs-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to file handle APIs.
const char kAllowNaClFileHandleAPI[]        = "allow-nacl-file-handle-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to TCP/UDP socket APIs.
const char kAllowNaClSocketAPI[]            = "allow-nacl-socket-api";

// Don't block outdated plugins.
const char kAllowOutdatedPlugins[]          = "allow-outdated-plugins";

// By default, an https page cannot run JavaScript, CSS or plug-ins from http
// URLs. This provides an override to get the old insecure behavior.
const char kAllowRunningInsecureContent[]   = "allow-running-insecure-content";

// Prevents Chrome from requiring authorization to run certain widely installed
// but less commonly used plug-ins.
const char kAlwaysAuthorizePlugins[]        = "always-authorize-plugins";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Specifies that the associated value should be launched in "application"
// mode.
const char kApp[]                           = "app";

// Specifies an URL to use for app list start page.
const char kAppListStartPageURL[]           = "app-list-start-page-url";

// Overrides the apps checkout URL, which is used to determine when to expose
// some private APIs.
const char kAppsCheckoutURL[]               = "apps-checkout-url";

// The URL that the webstore APIs download extensions from.
// Note: the URL must contain one '%s' for the extension ID.
const char kAppsGalleryDownloadURL[]        = "apps-gallery-download-url";

// A setting to cause extension/app installs from the webstore skip the normal
// confirmation dialog. A value of 'accept' means to always act as if the dialog
// was accepted, and 'cancel' means to always act as if the dialog was
// cancelled.
//
// TODO (rdevlin.cronin): Remove this.
// This is not a good use of a command-line flag, as it would be equally
// effective as a global boolean. Additionally, this opens up a dangerous way
// for attackers to append a commandline flag and circumvent all user action for
// installing an extension.
const char kAppsGalleryInstallAutoConfirmForTests[] =
    "apps-gallery-install-auto-confirm-for-tests";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[]                = "apps-gallery-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[]          = "apps-gallery-update-url";

// Value of GAIA auth code for --force-app-mode.
const char kAppModeAuthCode[]               = "app-mode-auth-code";

// Value of OAuth2 refresh token for --force-app-mode.
const char kAppModeOAuth2Token[]            = "app-mode-oauth-token";

// Whether to always use the new app install bubble when installing an app.
const char kAppsNewInstallBubble[]          = "apps-new-install-bubble";

// Experimental native frame support for packaged apps.
const char kAppsUseNativeFrame[]            = "apps-use-native-frame";

// Enables overriding the path for the default authentication extension.
const char kAuthExtensionPath[]             = "auth-ext-path";

// Whitelist of servers that Negotiate will generate delegated Kerberos tickets
// for.
const char kAuthNegotiateDelegateWhitelist[] =
    "auth-negotiate-delegate-whitelist";

// HTTP authentication schemes to enable. This is a comma-separated list of
// authentication schemes (basic, digest, ntlm, and negotiate). By default all
// schemes are enabled. The primary use of this command line flag is to help
// triage authentication-related issues reported by end-users.
const char kAuthSchemes[]                   = "auth-schemes";

// Whitelist of servers which NTLM and Negotiate can automatically authenticate
// with using the default credentials of the currently logged in user.
const char kAuthServerWhitelist[]           = "auth-server-whitelist";

// A flag that is used to tell Chrome that it was launched automatically at
// computer startup and not by some user action.
const char kAutoLaunchAtStartup[]           = "auto-launch-at-startup";

// Certificate Transparency: Uses the provided log(s) for checking Signed
// Certificate Timestamps provided with certificates.
// The switch's value is:
//   log_description:log_key,log_description:log_key,...
// where
//   log_description is a textual description of the log.
//   log_key is a Base64'd DER-encoded SubjectPublicKeyInfo of the log's
//   public key.
// Multiple logs can be specified by repeating description:key pairs,
// separated by a comma.
const char kCertificateTransparencyLog[] =
    "certificate-transparency-log";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Checks the cloud print connector policy, informing the service process if
// the policy is set to disallow the connector, then quits.
const char kCheckCloudPrintConnectorPolicy[] =
    "check-cloud-print-connector-policy";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[]          = "cipher-suite-blacklist";

// Tells chrome to display the cloud print dialog and upload the specified file
// for printing.
const char kCloudPrintFile[]                = "cloud-print-file";

// Specifies the mime type to be used when uploading data from the file
// referenced by cloud-print-file. Defaults to "application/pdf" if
// unspecified.
const char kCloudPrintFileType[]            = "cloud-print-file-type";

// Used with kCloudPrintFile to specify a JSON print ticket for the resulting
// print job. Defaults to null if unspecified.
const char kCloudPrintPrintTicket[]         = "cloud-print-print-ticket";

// Used with kCloudPrintFile to specify a title for the resulting print job.
const char kCloudPrintJobTitle[]            = "cloud-print-job-title";

// Setup cloud print proxy for provided printers. This does not start
// service or register proxy for autostart.
const char kCloudPrintSetupProxy[]          = "cloud-print-setup-proxy";

// The URL of the cloud print service to use, overrides any value stored in
// preferences, and the default. Only used if the cloud print service has been
// enabled. Used for testing.
const char kCloudPrintServiceURL[]          = "cloud-print-service";

// The XMPP endpoint the cloud print service will use. Only used if the cloud
// print service has been enabled. Used for testing.
const char kCloudPrintXmppEndpoint[] = "cloud-print-xmpp-endpoint";

// Comma-separated options to troubleshoot the component updater. Only valid
// for the browser process.
const char kComponentUpdater[]              = "component-updater";

// Causes the browser process to inspect loaded and registered DLLs for known
// conflicts and warn the user.
const char kConflictingModulesCheck[]       = "conflicting-modules-check";

// Comma-separated list of BrowserThreads that cause browser process to crash
// if the given browser thread is not responsive. UI,IO,DB,FILE,CACHE are the
// list of BrowserThreads that are supported.
//
// For example:
//    --crash-on-hang-threads=UI:3:18,IO:3:18 --> Crash the browser if UI or IO
//      is not responsive for 18 seconds and the number of browser threads that
//      are responding is less than or equal to 3.
const char kCrashOnHangThreads[]            = "crash-on-hang-threads";

// Some platforms like ChromeOS default to empty desktop.
// Browser tests may need to add this switch so that at least one browser
// instance is created on startup.
// TODO(nkostylev): Investigate if this switch could be removed.
// (http://crbug.com/148675)
const char kCreateBrowserOnStartupForTests[] =
    "create-browser-on-startup-for-tests";

// Enables a frame context menu item that toggles the frame in and out of glass
// mode (Windows Vista and up only).
const char kDebugEnableFrameToggle[]        = "debug-enable-frame-toggle";

// Adds debugging entries such as Inspect Element to context menus of packed
// apps.
const char kDebugPackedApps[]               = "debug-packed-apps";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
const char kDiagnosticsFormat[]             = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
const char kDiagnosticsRecovery[]           = "diagnostics-recovery";

// Disables the experimental asynchronous DNS client.
const char kDisableAsyncDns[]               = "disable-async-dns";

// Disables CNAME lookup of the host when generating the Kerberos SPN for a
// Negotiate challenge. See HttpAuthHandlerNegotiate::CreateSPN for more
// background.
const char kDisableAuthNegotiateCnameLookup[] =
    "disable-auth-negotiate-cname-lookup";

// Disables background mode (background apps will not keep chrome running in
// the background).
const char kDisableBackgroundMode[]         = "disable-background-mode";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
const char kDisableBackgroundNetworking[]   = "disable-background-networking";

// Disables the bundled PPAPI version of Flash.
const char kDisableBundledPpapiFlash[]      = "disable-bundled-ppapi-flash";

// Disables the client-side phishing detection feature. Note that even if
// client-side phishing detection is enabled, it will only be active if the
// user has opted in to UMA stats and SafeBrowsing is enabled in the
// preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

// Disable default component extensions with background pages - useful for
// performance tests where these pages may interfere with perf results.
const char kDisableComponentExtensionsWithBackgroundPages[] =
    "disable-component-extensions-with-background-pages";

const char kDisableComponentUpdate[]        = "disable-component-update";

// Disables establishing certificate revocation information by downloading a
// set of CRLs rather than performing on-line checks.
const char kDisableCRLSets[]                = "disable-crl-sets";

// Disables installation of default apps on first run. This is used during
// automated testing.
const char kDisableDefaultApps[]            = "disable-default-apps";

// Disables device discovery.
const char kDisableDeviceDiscovery[]        = "disable-device-discovery";

// Disables device discovery notifications.
const char kDisableDeviceDiscoveryNotifications[] =
    "disable-device-discovery-notifications";

// Disables Domain Reliability Monitoring.
const char kDisableDomainReliability[]      = "disable-domain-reliability";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";

// Disable the net::URLRequestThrottlerManager functionality for
// requests originating from extensions.
const char kDisableExtensionsHttpThrottling[] =
    "disable-extensions-http-throttling";

// Disable mandatory enforcement of web_accessible_resources in extensions.
const char kDisableExtensionsResourceWhitelist[] =
    "disable-extensions-resource-whitelist";

// Disables embedding of Flash fullscreen widgets within the browser window.
// This restores the old code paths where Flash fullscreen would display in its
// own separate, always-on-top window.  In addition, this disables the new logic
// which would prevent fullscreening the browser window during screen-capture of
// a tab.  http://crbug.com/256870 and http://crbug.com/290403
const char kDisableFullscreenWithinTab[] = "disable-fullscreen-within-tab";

// Prevent infobars from appearing.
const char kDisableInfoBars[]               = "disable-infobars";

// Don't resolve hostnames to IPv6 addresses. This can be used when debugging
// issues relating to IPv6, but shouldn't otherwise be needed. Be sure to file
// bugs if something isn't working properly in the presence of IPv6. This flag
// can be overidden by the "enable-ipv6" flag.
const char kDisableIPv6[]                   = "disable-ipv6";

// Disable the behavior that the second click on a launcher item (the click when
// the item is already active) minimizes the item.
const char kDisableMinimizeOnSecondLauncherItemClick[] =
    "disable-minimize-on-second-launcher-item-click";

// Disables the menu on the NTP for accessing sessions from other devices.
const char kDisableNTPOtherSessionsMenu[]   = "disable-ntp-other-sessions-menu";

// Disable the origin chip.
const char kDisableOriginChip[]             = "disable-origin-chip";

// Disable the origin chip in the location bar.
const char kDisableOriginChipV2[]             = "disable-origin-chip-v2";

// Disable the setting to prompt the user for their OS account password before
// revealing plaintext passwords in the password manager.
const char kDisablePasswordManagerReauthentication[] =
    "disable-password-manager-reauthentication";

// Enables searching for people from the apps list search box.
const char kDisablePeopleSearch[]           = "disable-people-search";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Disables the usage of Portable Native Client.
const char kDisablePnacl[]                  = "disable-pnacl";

// Disable speculative TCP/IP preconnection.
const char kDisablePreconnect[]             = "disable-preconnect";

// Disable prerendering based on local browsing history.
const char kDisablePrerenderLocalPredictor[] =
    "disable-prerender-local-predictor";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disables support for the QUIC protocol.
const char kDisableQuic[]                   = "disable-quic";

// Disables support for the HTTPS over QUIC protocol.  This is a temporary
// testing flag.  This only has an effect if QUIC protocol is enabled.
const char kDisableQuicHttps[]              = "disable-quic-https";

// Disable use of pacing of QUIC packets.
// This only has an effect if QUIC protocol is enabled.
const char kDisableQuicPacing[]             = "disable-quic-pacing";

// Disable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if QUIC protocol is enabled.
const char kDisableQuicPortSelection[]      = "disable-quic-port-selection";

// Prevents the URLs of BackgroundContents from being remembered and
// re-launched when the browser restarts.
const char kDisableRestoreBackgroundContents[] =
    "disable-restore-background-contents";

// Prevents the save password bubble from being enabled.
const char kDisableSavePasswordBubble[] = "disable-save-password-bubble";

// Disables throttling prints initiated by scripts.
const char kDisableScriptedPrintThrottling[] =
    "disable-scripted-print-throttling";

// Disables the "search button in omnibox" experiment.
const char kDisableSearchButtonInOmnibox[]  =
    "disable-search-button-in-omnibox";

// Disable SPDY/3.1. This is a temporary testing flag.
const char kDisableSpdy31[]                 = "disable-spdy31";

// Disables syncing browser data to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Disables sync/API based session sync implementation (back to legacy).
const char kDisableSyncSessionsV2[] = "disable-sync-sessions-v2";

// Disable synced notifications.
const char kDisableSyncSyncedNotifications[] =
    "disable-sync-synced-notifications";

// Disables syncing one or more sync data types that are on by default.
// See sync/internal_api/public/base/model_type.h for possible types. Types
// should be comma separated, and follow the naming convention for string
// representation of model types, e.g.:
// --disable-synctypes='Typed URLs, Bookmarks, Autofill Profiles'
const char kDisableSyncTypes[]              = "disable-sync-types";

// Disables TLS Channel ID extension.
const char kDisableTLSChannelID[]           = "disable-tls-channel-id";

// Disables some security measures when accessing user media devices like
// webcams and microphones, especially on non-HTTPS pages.
const char kDisableUserMediaSecurity[]      = "disable-user-media-security";

// Disables the backend service for web resources.
const char kDisableWebResources[]           = "disable-web-resources";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/119175)
const char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Disables prefetching of DNS information.
const char kDnsPrefetchDisable[]            = "dns-prefetch-disable";

// Requests that a running browser process dump its collected histograms to a
// given file. The file is overwritten if it exists.
const char kDumpBrowserHistograms[]         = "dump-browser-histograms";

// Overrides the path of Easy Unlock component app.
extern const char kEasyUnlockAppPath[]      = "easy-unlock-app-path";

// Enables the <adview> tag in packaged apps.
const char kEnableAdview[]                  = "enable-adview";

// If set, the app list will be enabled as if enabled from CWS.
const char kEnableAppList[]                 = "enable-app-list";

// Enables the <window-controls> tag in platform apps.
const char kEnableAppWindowControls[]       = "enable-app-window-controls";

// Show apps windows after the first paint. Windows will be shown significantly
// later for heavy apps loading resources synchronously but it will be
// insignificant for apps that load most of their resources asynchronously.
const char kEnableAppsShowOnFirstPaint[]    = "enable-apps-show-on-first-paint";

// Enables the experimental asynchronous DNS client.
const char kEnableAsyncDns[]                = "enable-async-dns";

// Enables the inclusion of non-standard ports when generating the Kerberos SPN
// in response to a Negotiate challenge. See
// HttpAuthHandlerNegotiate::CreateSPN for more background.
const char kEnableAuthNegotiatePort[]       = "enable-auth-negotiate-port";

// Enables the pre- and auto-login features. When a user signs in to sync, the
// browser's cookie jar is pre-filled with GAIA cookies. When the user visits a
// GAIA login page, an info bar can help the user login.
const char kEnableAutologin[]               = "enable-autologin";

// Enables the Automation extension API.
// TODO(dtseng): Remove once API enabled for stable channel.
const char kEnableAutomationAPI[]           = "enable-automation-api";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enables client hints, which adds hints about browser state to HTTP requests.
const char kEnableClientHints[]             = "enable-client-hints";

// Enables the multi-level undo system for bookmarks.
const char kEnableBookmarkUndo[]            = "enable-bookmark-undo";

// This applies only when the process type is "service". Enables the Cloud
// Print Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// If true devtools experimental settings are enabled.
const char kEnableDevToolsExperiments[]     = "enable-devtools-experiments";

// Enable device discovery notifications.
const char kEnableDeviceDiscoveryNotifications[] =
    "enable-device-discovery-notifications";

// Enables the DOM distiller.
const char kEnableDomDistiller[]               = "enable-dom-distiller";

// Enables Domain Reliability Monitoring.
const char kEnableDomainReliability[]          = "enable-domain-reliability";

// Enable Enhanced Bookmarks.
const char kEnhancedBookmarksExperiment[] = "enhanced-bookmarks-experiment";

// Enables Easy Unlock to be set up and used.
extern const char kEnableEasyUnlock[] = "enable-easy-unlock";

// Enables experimentation with ephemeral apps, which are launched without
// installing in Chrome.
const char kEnableEphemeralApps[]           = "enable-ephemeral-apps";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

const char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Enable the fast unload controller, which speeds up tab/window close by
// running a tab's onunload js handler independently of the GUI -
// crbug.com/142458 .
const char kEnableFastUnload[]         = "enable-fast-unload";

// Enables IPv6 support, even if probes suggest that it may not be fully
// supported. Some probes may require internet connections, and this flag will
// allow support independent of application testing. This flag overrides
// "disable-ipv6" which appears elswhere in this file.
const char kEnableIPv6[]                    = "enable-ipv6";

// Enables experimentation with launching ephemeral apps via hyperlinks.
const char kEnableLinkableEphemeralApps[]   = "enable-linkable-ephemeral-apps";

// Enables metrics recording and reporting in the browser startup sequence, as
// if this was an official Chrome build where the user allowed metrics
// reporting. This is used for testing only.
const char kEnableMetricsReportingForTesting[] =
    "enable-metrics-reporting-for-testing";

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enables the network-related benchmarking extensions.
const char kEnableNetBenchmarking[]         = "enable-net-benchmarking";

// Enables |NetworkTimeService| to convert local time to network time.
const char kEnableNetworkTime[]             = "enable-network-time";

// Enables NPN with HTTP. It means NPN is enabled but SPDY won't be used.
// HTTP is still used for all requests.
const char kEnableNpnHttpOnly[]             = "enable-npn-http";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[]       = "enable-offline-auto-reload";

// Controls which branch of the origin chip experiment is enabled. The first
// flag (enable-origin-chip) is equivalent to the third
// (enable-origin-chip-trailing-location-bar) and exists for backwards
// compatability with an earlier version of the experiment.
//
// We're using independent flags here (as opposed to a common flag with
// different values) to be able to enable/disable the entire experience
// associated with this feature server-side from the FieldTrial (the complete
// experience includes other flag changes as well). It is not currently possible
// to do that with "flag=value" flags.
const char kEnableOriginChip[] = "enable-origin-chip";
const char kEnableOriginChipLeadingLocationBar[] =
    "enable-origin-chip-leading-location-bar";
const char kEnableOriginChipTrailingLocationBar[] =
    "enable-origin-chip-trailing-location-bar";
const char kEnableOriginChipLeadingMenuButton[] =
    "enable-origin-chip-leading-menu-button";

// Controls which branch of the origin chip in location bar experiment is
// enabled.
//
// We're using independent flags here (as opposed to a common flag with
// different values) to be able to enable/disable the entire experience
// associated with this feature server-side from the FieldTrial (the complete
// experience includes other flag changes as well). It is not currently possible
// to do that with "flag=value" flags.
const char kEnableOriginChipV2[] = "enable-origin-chip-v2";
const char kEnableOriginChipV2HideOnMouseRelease[] =
    "enable-origin-chip-v2-hide-on-mouse-release";
const char kEnableOriginChipV2HideOnUserInput[] =
    "enable-origin-chip-v2-hide-on-user-input";

// Enables panels (always on-top docked pop-up windows).
const char kEnablePanels[]                  = "enable-panels";

// Enables showing unregistered printers in print preview
const char kEnablePrintPreviewRegisterPromos[] =
    "enable-print-preview-register-promos";

// Enable Privet storage.
const char kEnablePrivetStorage[]     = "enable-privet-storage";

// Enables tracking of tasks in profiler for viewing via about:profiler.
// To predominantly disable tracking (profiling), use the command line switch:
// --enable-profiling=0
// Some tracking will still take place at startup, but it will be turned off
// during chrome_browser_main.
const char kEnableProfiling[]               = "enable-profiling";

// Enables query in the omnibox.
const char kEnableQueryExtraction[]         = "enable-query-extraction";

// Enables support for the QUIC protocol.  This is a temporary testing flag.
const char kEnableQuic[]                    = "enable-quic";

// Enables support for the HTTPS over QUIC protocol.  This is a temporary
// testing flag.  This only has an effect if QUIC protocol is enabled.
const char kEnableQuicHttps[]               = "enable-quic-https";

// Disable use of pacing of QUIC packets.
// This only has an effect if QUIC protocol is enabled.
const char kEnableQuicPacing[]              = "enable-quic-pacing";

// Enable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if QUIC protocol is enabled.
const char kEnableQuicPortSelection[]       = "enable-quic-port-selection";

// Enables save password prompt bubble.
const char kEnableSavePasswordBubble[]      = "enable-save-password-bubble";

// Enables SDCH for https schemes.
const char kEnableSdchOverHttps[] = "enable-sdch-over-https";

// Controls which branch of the "search button in omnibox" experiment is
// enabled.
//
// We're using independent flags here (as opposed to a common flag with
// different values) to be able to enable/disable the entire experience
// associated with this feature server-side from the FieldTrial (the complete
// experience includes other flag changes as well). It is not currently possible
// to do that with "flag=value" flags.
const char kEnableSearchButtonInOmniboxAlways[] =
    "enable-search-button-in-omnibox-always";
const char kEnableSearchButtonInOmniboxForStr[] =
    "enable-search-button-in-omnibox-for-str";
const char kEnableSearchButtonInOmniboxForStrOrIip[] =
    "enable-search-button-in-omnibox-for-str-or-iip";

// Enable settings in a separate browser window per profile.
const char kEnableSettingsWindow[]          = "enable-settings-window";

// Enable SPDY/4, aka HTTP/2. This is a temporary testing flag.
const char kEnableSpdy4[]                   = "enable-spdy4";

// Enables auto correction for misspelled words.
const char kEnableSpellingAutoCorrect[]     = "enable-spelling-auto-correct";

// Enables participation in the field trial for user feedback to spelling
// service.
const char kEnableSpellingFeedbackFieldTrial[] =
    "enable-spelling-feedback-field-trial";

// Enables the stacked tabstrip.
const char kEnableStackedTabStrip[]         = "enable-stacked-tab-strip";

// Enables an experimental hosted app experience.
const char kEnableStreamlinedHostedApps[]   = "enable-streamlined-hosted-apps";

// Enables synced notifications.
const char kEnableSyncSyncedNotifications[] =
    "enable-sync-synced-notifications";

// Enables synced articles.
const char kEnableSyncArticles[]            = "enable-sync-articles";

// Enables fanciful thumbnail processing. Used with NTP for
// instant-extended-api, where thumbnails are generally smaller.
const char kEnableThumbnailRetargeting[]   = "enable-thumbnail-retargeting";

// Enables Translate experimental new UX which replaces the infobar.
const char kEnableTranslateNewUX[]         = "enable-translate-new-ux";

// Enables Alternate-Protocol when the port is user controlled (> 1024).
const char kEnableUserAlternateProtocolPorts[] =
    "enable-user-controlled-alternate-protocol-ports";

// Spawns threads to watch for excessive delays in specified message loops.
// User should set breakpoints on Alarm() to examine problematic thread.
//
// Usage:   -enable-watchdog=[ui][io]
//
// Order of the listed sub-arguments does not matter.
const char kEnableWatchdog[]                = "enable-watchdog";

// Uses WebSocket over SPDY.
const char kEnableWebSocketOverSpdy[]       = "enable-websocket-over-spdy";

// Explicitly allows additional ports using a comma-separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

// Turns on extension install verification if it would not otherwise have been
// turned on.
const char kExtensionsInstallVerification[] = "extensions-install-verification";

// Specifies a comma-separated list of extension ids that should be forced to
// be treated as not from the webstore when doing install verification.
const char kExtensionsNotWebstore[] = "extensions-not-webstore";

// Frequency in seconds for Extensions auto-update.
const char kExtensionsUpdateFrequency[]     = "extensions-update-frequency";

// Additional query params to insert in the search and instant URLs.  Useful for
// testing.
const char kExtraSearchQueryParams[]        = "extra-search-query-params";

// Fakes the channel of the browser for purposes of Variations filtering. This
// is to be used for testing only. Possible values are "stable", "beta", "dev"
// and "canary". Note that this only applies if the browser's reported channel
// is UNKNOWN.
const char kFakeVariationsChannel[]         = "fake-variations-channel";

// If this flag is present then this command line is being delegated to an
// already running chrome process via the fast path, ie: before chrome.dll is
// loaded. It is useful to tell the difference for tracking purposes.
const char kFastStart[]            = "fast-start";

// These two flags are added around the switches about:flags adds to the
// command line. This is useful to see which switches were added by about:flags
// on about:version. They don't have any effect.
const char kFlagSwitchesBegin[]             = "flag-switches-begin";
const char kFlagSwitchesEnd[]               = "flag-switches-end";

// Alternative feedback server to use when submitting user feedback
const char kFeedbackServer[]                = "feedback-server";

// The file descriptor limit is set to the value of this switch, subject to the
// OS hard limits. Useful for testing that file descriptor exhaustion is
// handled gracefully.
const char kFileDescriptorLimit[]           = "file-descriptor-limit";

// Forces application mode. This hides certain system UI elements and forces
// the app to be installed if it hasn't been already.
const char kForceAppMode[]                  = "force-app-mode";

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
const char kForceFirstRun[]                 = "force-first-run";

// Forces additional Chrome Variation Ids that will be sent in X-Client-Data
// header, specified as a 64-bit encoded list of numeric experiment ids. Ids
// prefixed with the character "t" will be treated as Trigger Variation Ids.
const char kForceVariationIds[]             = "force-variation-ids";

// Specifies an alternate URL to use for speaking to Google. Useful for testing.
const char kGoogleBaseURL[]                 = "google-base-url";

// Specifies a custom name for the GSSAPI library to load.
const char kGSSAPILibraryName[]             = "gssapi-library-name";

// These flags show the man page on Linux. They are equivalent to each
// other.
const char kHelp[]                          = "help";
const char kHelpShort[]                     = "h";

// Makes Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This only
// shows an error box because the only way to hide Chrome is by uninstalling
// it.
const char kHideIcons[]                     = "hide-icons";

// Enables grouping websites by domain and filtering them by period.
const char kHistoryEnableGroupByDomain[]    = "enable-grouped-history";

// Specifies which page will be displayed in newly-opened tabs. We need this
// for testing purposes so that the UI tests don't depend on what comes up for
// http://google.com.
const char kHomePage[]                      = "homepage";

// Comma-separated list of rules that control how hostnames are mapped.
//
// For example:
//    "MAP * 127.0.0.1" --> Forces all hostnames to be mapped to 127.0.0.1
//    "MAP *.google.com proxy" --> Forces all google.com subdomains to be
//                                 resolved to "proxy".
//    "MAP test.com [::1]:77 --> Forces "test.com" to resolve to IPv6 loopback.
//                               Will also force the port of the resulting
//                               socket address to be 77.
//    "MAP * baz, EXCLUDE www.google.com" --> Remaps everything to "baz",
//                                            except for "www.google.com".
//
// These mappings apply to the endpoint host in a net::URLRequest (the TCP
// connect and host resolver in a direct connection, and the CONNECT in an http
// proxy connection, and the endpoint host in a SOCKS proxy connection).
const char kHostRules[]                     = "host-rules";

// The maximum number of concurrent host resolve requests (i.e. DNS) to allow
// (not counting backup attempts which would also consume threads).
// --host-resolver-retry-attempts must be set to zero for this to be exact.
const char kHostResolverParallelism[]       = "host-resolver-parallelism";

// The maximum number of retry attempts to resolve the host. Set this to zero
// to disable host resolver retry attempts.
const char kHostResolverRetryAttempts[]     = "host-resolver-retry-attempts";

// Causes net::URLFetchers to ignore requests for SSL client certificates,
// causing them to attempt an unauthenticated SSL/TLS session. This is intended
// for use when testing various service URLs (eg: kPromoServerURL, kSbURLPrefix,
// kSyncServiceURL, etc)
const char kIgnoreUrlFetcherCertRequests[]  =
    "ignore-urlfetcher-cert-requests";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Causes Chrome to initiate an installation flow for the given app.
const char kInstallChromeApp[]              = "install-chrome-app";

// Causes Chrome to attempt to get metadata from the webstore for the
// app/extension ID given, and then prompt the user to download and install it.
const char kInstallFromWebstore[]           = "install-from-webstore";

// Marks a renderer as an Instant process.
const char kInstantProcess[]                = "instant-process";

// Invalidation service should use GCM network channel even if experiment is not
// enabled.
const char kInvalidationUseGCMChannel[]     = "invalidation-use-gcm-channel";

// Specifies the testcase used by the IPC fuzzer.
const char kIpcFuzzerTestcase[]             = "ipc-fuzzer-testcase";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[]              = "keep-alive-for-test";

// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";

// Print automatically in kiosk mode. |kKioskMode| must be set as well.
// See http://crbug.com/31395.
const char kKioskModePrinting[]             = "kiosk-printing";

// Causes Chrome to attempt to get metadata from the webstore for the
// given item, and then prompt the user to download and install it.
const char kLimitedInstallFromWebstore[]    = "limited-install-from-webstore";

// Comma-separated list of directories with component extensions to load.
const char kLoadComponentExtension[]        = "load-component-extension";

// Loads an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Makes Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Sets the managed user ID for any loaded or newly created profile to the
// given value. Pass an empty string to mark the profile as non-supervised.
// Used for testing.
const char kManagedUserId[]                 = "managed-user-id";

// Used to authenticate requests to the Sync service for managed users. Setting
// this switch also causes Sync to be set up for a managed user.
const char kManagedUserSyncToken[]          = "managed-user-sync-token";

// Use to opt-in user into Finch experiment groups.
const char kManualEnhancedBookmarks[] = "manual-enhanced-bookmarks";
const char kManualEnhancedBookmarksOptout[] =
    "manual-enhanced-bookmarks-optout";

// Forces the maximum disk space to be used by the media cache, in bytes.
const char kMediaCacheSize[]                = "media-cache-size";

// Enables dynamic loading of the Memory Profiler DLL, which will trace all
// memory allocations during the run.
const char kMemoryProfiling[]               = "memory-profile";

// Enables histograming of tasks served by MessageLoop. See
// about:histograms/Loop for results, which show frequency of messages on each
// thread, including APC count, object signalling count, etc.
const char kMessageLoopHistogrammer[]       = "message-loop-histogrammer";

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kMetricsRecordingOnly[]          = "metrics-recording-only";

// Enables multiprofile Chrome.
const char kMultiProfiles[]                 = "multi-profiles";

// Sets the base logging level for the net log. Log 0 logs the most data.
// Intended primarily for use with --log-net-log.
const char kNetLogLevel[]                   = "net-log-level";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// By default, an https page can load images, fonts or frames from an http
// page. This switch overrides this to block this lesser mixed-content problem.
const char kNoDisplayingInsecureContent[]   = "no-displaying-insecure-content";

// Don't record/playback events when using record & playback.
const char kNoEvents[]                      = "no-events";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
const char kNoExperiments[]                 = "no-experiments";

// Skip First Run tasks, whether or not it's actually the First Run. Overridden
// by kForceFirstRun. This does not drop the First Run sentinel and thus doesn't
// prevent first run from occuring the next time chrome is launched without this
// flag.
const char kNoFirstRun[]                    = "no-first-run";

// Support a separate switch that enables the v8 playback extension.
// The extension causes javascript calls to Date.now() and Math.random()
// to return consistent values, such that subsequent loads of the same
// page will result in consistent js-generated data and XHR requests.
// Pages may still be able to generate inconsistent data from plugins.
const char kNoJsRandomness[]                = "no-js-randomness";

// Whether or not the browser should warn if the profile is on a network share.
// This flag is only relevant for Windows currently.
const char kNoNetworkProfileWarning[]       = "no-network-profile-warning";

// Don't send hyperlink auditing pings
const char kNoPings[]                       = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Disables the service process from adding itself as an autorun process. This
// does not delete existing autorun registrations, it just prevents the service
// from registering a new one.
const char kNoServiceAutorun[]              = "no-service-autorun";

// Does not automatically open a browser window on startup (used when
// launching Chrome for the purpose of hosting background apps).
const char kNoStartupWindow[]               = "no-startup-window";

// Disables checking whether we received an acknowledgment when registering
// a supervised user. Also disables the timeout during registration that waits
// for the ack. Useful when debugging against a server that does not
// support notifications.
const char kNoManagedUserAcknowledgmentCheck[]  =
    "no-managed-user-acknowledgment-check";

// Specifies the maximum number of threads to use for running the Proxy
// Autoconfig (PAC) script.
const char kNumPacThreads[]                 = "num-pac-threads";

// Launches URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Force use of QUIC for requests to the specified origin.
const char kOriginToForceQuicOn[]           = "origin-to-force-quic-on";

// The time that a new chrome process which is delegating to an already running
// chrome process started. (See ProcessSingleton for more details.)
const char kOriginalProcessStartTime[]      = "original-process-start-time";

// Enable the out of process PDF plugin.
const char kOutOfProcessPdf[] = "out-of-process-pdf";

// Packages an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Launches PerformanceMonitor at startup, which will gather statistics about
// Chrome's CPU and memory usage, page load times, startup times, and network
// usage, and will also store information about events which may be of interest,
// such as extension-related occurrences and crashes. Optionally, this may be
// run with an integer value representing the interval between the timed
// metric gatherings, measured in seconds (if invalid or not provided, the
// default interval is used).
const char kPerformanceMonitorGathering[]   = "performance-monitor-gathering";

// Read previously recorded data from the cache. Only cached data is read.
// See kRecordMode.
const char kPlaybackMode[]                  = "playback-mode";

// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[]                = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[]             = "ppapi-flash-version";

// Triggers prerendering of search base page to prefetch results for the typed
// omnibox query. Only has an effect when prerender is enabled.
const char kPrefetchSearchResults[]         = "prefetch-search-results";

// Triggers prerendering of pages from suggestions in the omnibox. Only has an
// effect when Instant is either disabled or restricted to search, and when
// prerender is enabled.
const char kPrerenderFromOmnibox[]          = "prerender-from-omnibox";
// These are the values the kPrerenderFromOmnibox switch may have, as in
// "--prerender-from-omnibox=auto". auto: Allow field trial selection.
const char kPrerenderFromOmniboxSwitchValueAuto[] = "auto";
//   disabled: No prerendering.
const char kPrerenderFromOmniboxSwitchValueDisabled[] = "disabled";
//   enabled: Guaranteed prerendering.
const char kPrerenderFromOmniboxSwitchValueEnabled[] = "enabled";
// Controls speculative prerendering of pages, and content prefetching. Both
// are dispatched from <link rel=prefetch href=...> elements.
const char kPrerenderMode[]                 = "prerender";
// These are the values the kPrerenderMode switch may have, as in
// "--prerender=auto".
//   auto: Allow field trial selection for prerender.
const char kPrerenderModeSwitchValueAuto[]  = "auto";
//   disabled: No prerendering.
const char kPrerenderModeSwitchValueDisabled[] = "disabled";
//   enabled: Prerendering.
const char kPrerenderModeSwitchValueEnabled[] = "enabled";

// Use IPv6 only for privet HTTP.
const char kPrivetIPv6Only[]                   = "privet-ipv6-only";

// Outputs the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
const char kProductVersion[]                = "product-version";

// Selects directory of profile to associate with the first browser launched.
const char kProfileDirectory[]              = "profile-directory";

// Starts the sampling based profiler for the browser process at startup. This
// will only work if chrome has been built with the gyp variable profiling=1.
// The output will go to the value of kProfilingFile.
const char kProfilingAtStart[]              = "profiling-at-start";

// Specifies a location for profiling output. This will only work if chrome has
// been built with the gyp variable profiling=1.
//
//   {pid} if present will be replaced by the pid of the process.
//   {count} if present will be incremented each time a profile is generated
//           for this process.
// The default is chrome-profile-{pid}.
const char kProfilingFile[]                 = "profiling-file";

// Specifies a path for the output of task-level profiling which can be loaded
// and viewed in about:profiler.
const char kProfilingOutputFile[]           = "profiling-output-file";

// Controls whether profile data is periodically flushed to a file. Normally
// the data gets written on exit but cases exist where chrome doesn't exit
// cleanly (especially when using single-process). A time in seconds can be
// specified.
const char kProfilingFlush[]                = "profiling-flush";

// Specifies a custom URL for fetching NTP promo data.
const char kPromoServerURL[]                = "promo-server-url";

// Forces proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[]               = "proxy-bypass-list";

// Uses the pac script at the given URL
const char kProxyPacUrl[]                   = "proxy-pac-url";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[]                   = "proxy-server";

// Specifies the maximum length for a QUIC packet.
const char kQuicMaxPacketLength[]           = "quic-max-packet-length";

// Specifies the version of QUIC to use.
const char kQuicVersion[]                   = "quic-version";

// Chrome supports a playback and record mode.  Record mode saves *everything*
// to the cache.  Playback mode reads data exclusively from the cache.  This
// allows us to record a session into the cache and then replay it at will.
// See also kPlaybackMode.
const char kRecordMode[]                    = "record-mode";

// Enables print preview in the renderer. This flag is generated internally by
// Chrome and does nothing when directly passed to the browser.
const char kRendererPrintPreview[]          = "renderer-print-preview";

// If set, the app list will forget it has been installed on startup. Note this
// doesn't prevent the app list from running, it just makes Chrome think the app
// list hasn't been enabled (as in kEnableAppList) yet.
const char kResetAppListInstallState[]      = "reset-app-list-install-state";

// Forces a reset of the one-time-randomized FieldTrials on this client, also
// known as the Chrome Variations state.
const char kResetVariationState[]           = "reset-variation-state";

// Indicates the last session should be restored on startup. This overrides the
// preferences value and is primarily intended for testing. The value of this
// switch is the number of tabs to wait until loaded before 'load completed' is
// sent to the ui_test.
const char kRestoreLastSession[]            = "restore-last-session";

// Disable saving pages as HTML-only, disable saving pages as HTML Complete
// (with a directory of sub-resources). Enable only saving pages as MHTML.
// See http://crbug.com/120416 for how to remove this switch.
const char kSavePageAsMHTML[]               = "save-page-as-mhtml";

// If present, safebrowsing only performs update when
// SafeBrowsingProtocolManager::ForceScheduleNextUpdate() is explicitly called.
// This is used for testing only.
const char kSbDisableAutoUpdate[] = "safebrowsing-disable-auto-update";

// TODO(lzheng): Remove this flag once the feature works fine
// (http://crbug.com/74848).
//
// Disables safebrowsing feature that checks download url and downloads
// content's hash to make sure the content are not malicious.
const char kSbDisableDownloadProtection[] =
    "safebrowsing-disable-download-protection";

// Disables safebrowsing feature that checks for blacklisted extensions.
const char kSbDisableExtensionBlacklist[] =
    "safebrowsing-disable-extension-blacklist";

// Disables safebrowsing feature that provides a side-effect free whitelist.
const char kSbDisableSideEffectFreeWhitelist[] =
    "safebrowsing-disable-side-effect-free-whitelist";

// Causes the process to run as a service process.
const char kServiceProcess[]                = "service";

// Sets a token in the token service, for testing.
const char kSetToken[]                      = "set-token";

// If true the app list will be shown.
const char kShowAppList[]                   = "show-app-list";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Marks a renderer as the signin process.
const char kSigninProcess[]                 = "signin-process";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
const char kSilentDebuggerExtensionAPI[]    = "silent-debugger-extension-api";

// Changes the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when gyp dcheck_always_on=1.
const char kSilentDumpOnDCHECK[]            = "silent-dump-on-dcheck";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
const char kSilentLaunch[]                  = "silent-launch";

// Simulates an update being available.
const char kSimulateUpgrade[]               = "simulate-upgrade";

// Simulates a critical update being available.
const char kSimulateCriticalUpdate[]        = "simulate-critical-update";

// Simulates that current version is outdated.
const char kSimulateOutdated[]               = "simulate-outdated";

// Simulates that current version is outdated and auto-update is off.
const char kSimulateOutdatedNoAU[]           = "simulate-outdated-no-au";

// Specifies the URL where spelling service feedback data will be sent instead
// of the default URL. This switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackUrl[] = "spelling-service-feedback-url";

// Specifies the number of seconds between sending batches of feedback to
// spelling service. The default is 30 minutes. The mininum is 5 seconds. This
// switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackIntervalSeconds[] =
    "spelling-service-feedback-interval-seconds";

// Specifies the maximum SSL/TLS version ("ssl3", "tls1", "tls1.1", or
// "tls1.2").
const char kSSLVersionMax[]                 = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("ssl3", "tls1", "tls1.1", or
// "tls1.2").
const char kSSLVersionMin[]                 = "ssl-version-min";

// Starts the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Controls the width of time-of-day filters on the 'suggested' ntp page, in
// minutes.
const char kSuggestionNtpFilterWidth[]      = "suggestion-ntp-filter-width";

// Enables a normal distribution dropoff to the relevancy of visits with respect
// to the time of day.
const char kSuggestionNtpGaussianFilter[]   = "suggestion-ntp-gaussian-filter";

// Enables a linear dropoff to the relevancy of visits with respect to the time
// of day.
const char kSuggestionNtpLinearFilter[]     = "suggestion-ntp-linear-filter";

// Allows insecure XMPP connections for sync (for testing).
const char kSyncAllowInsecureXmppConnection[] =
    "sync-allow-insecure-xmpp-connection";

// Invalidates any login info passed into sync's XMPP connection.
const char kSyncInvalidateXmppLogin[]       = "sync-invalidate-xmpp-login";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// Overrides the default host:port used for sync notifications.
const char kSyncNotificationHostPort[]      = "sync-notification-host-port";

// Overrides the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Makes the sync code to throw an unrecoverable error after initialization.
// Useful for testing unrecoverable error scenarios.
const char kSyncThrowUnrecoverableError[]   = "sync-throw-unrecoverable-error";


// Tries to connect to XMPP using SSLTCP first (for testing).
const char kSyncTrySsltcpFirstForXmpp[]     = "sync-try-ssltcp-first-for-xmpp";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[]     = "sync-disable-deferred-startup";

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables feature to avoid unnecessary GetUpdate requests.
const char kSyncEnableGetUpdateAvoidance[]   =
    "sync-enable-get-update-avoidance";

// Enables directory support for sync filesystem
const char kSyncfsEnableDirectoryOperation[] =
    "enable-syncfs-directory-operation";

// Passes the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Disables same-origin check on HTTP resources pushed via a SPDY proxy.
// The value is the host:port of the trusted proxy.
const char kTrustedSpdyProxy[]              = "trusted-spdy-proxy";

// Experimental. Shows a dialog asking the user to try chrome. This flag is to
// be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Uninstalls an extension with the specified extension id.
const char kUninstallExtension[]            = "uninstall-extension";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Overrides per-origin quota settings to unlimited storage for any
// apps/origins.  This should be used only for testing purpose.
const char kUnlimitedStorage[]              = "unlimited-storage";

// Uses Spdy for the transport protocol instead of HTTP. This is a temporary
// testing flag.
const char kUseSpdy[]                       = "use-spdy";

// Disables use of the spelling web service and only provides suggestions.
// This will only work if asynchronous spell checking is not disabled.
const char kUseSpellingSuggestions[]        = "use-spelling-suggestions";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// Examines a .crx for validity and prints the result.
const char kValidateCrx[]                   = "validate-crx";

// Uses experimental simple cache backend if possible.
const char kUseSimpleCacheBackend[]         = "use-simple-cache-backend";

// Specifies a custom URL for the server which reports variation data to the
// client. Specifying this switch enables the Variations service on
// unofficial builds. See variations_service.cc.
const char kVariationsServerURL[]           = "variations-server-url";

// Prints version information and quits.
const char kVersion[]                       = "version";

// Specify the initial window position: --window-position=x,y
const char kWindowPosition[]                = "window-position";

// Specify the initial window size: --window-size=w,h
const char kWindowSize[]                    = "window-size";

// Uses WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is to
// use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

#if defined(ENABLE_PLUGIN_INSTALLATION)
// Specifies a custom URL for fetching plug-ins metadata. Used for testing.
const char kPluginsMetadataServerURL[]      = "plugins-metadata-server-url";
#endif

#if defined(OS_ANDROID)
// Disables the app banner <meta> tag.
const char kDisableAppBanners[]              = "disable-app-banners";

// Disables support for playing videos on Chromecast devices.
const char kDisableCast[]                    = "disable-cast";

// Disables the new NTP.
const char kDisableNewNTP[]                  = "disable-new-ntp";

// Disables zero suggest experiment on Dev channel.
const char kDisableZeroSuggest[] = "disable-zero-suggest";

// Enable the accessibility tab switcher.
const char kEnableAccessibilityTabSwitcher[] =
    "enable-accessibility-tab-switcher";

// Enables Contextual Search
const char kEnableContextualSearch[]        = "contextual-search";

// Enables the new NTP.
const char kEnableNewNTP[]                  = "enable-new-ntp";

// Enables zero suggest functionality on Dev channel, showing contextual
// suggestions (EtherSuggest) for http pages and google.com search queries.
const char kEnableZeroSuggestEtherSerp[] =
    "enable-zero-suggest-ether-serp";

// Enables zero suggest functionality on Dev channel, showing contextual
// suggestions (EtherSuggest) for http pages.
const char kEnableZeroSuggestEtherNoSerp[] =
    "enable-zero-suggest-ether-noserp";

// Enables zero suggest functionality on Dev channel, showing most visited
// sites as default suggestions.
const char kEnableZeroSuggestMostVisited[] =
    "enable-zero-suggest-most-visited";

// Enables zero suggest functionality on Dev channel, showing recently typed
// queries as default suggestions.
const char kEnableZeroSuggestPersonalized[] =
    "enable-zero-suggest-personalized";

// Enables instant search clicks feature.
const char kEnableInstantSearchClicks[] = "enable-instant-search-clicks";

#endif

#if defined(USE_ASH)
const char kOpenAsh[]                       = "open-ash";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// Specifies which password store to use (detect, default, gnome, kwallet).
const char kPasswordStore[]                 = "password-store";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Triggers migration of user data directory to another directory
// specified as a parameter. The migration is done under singleton lock,
// and sanity checks are made to avoid corrupting the profile.
// The browser exits after migration is complete.
const char kMigrateDataDirForSxS[]          = "migrate-data-dir-for-sxs";

// Allows sending text-to-speech requests to speech-dispatcher, a common
// Linux speech service. Because it's buggy, the user must explicitly
// enable it so that visiting a random webpage can't cause instability.
const char kEnableSpeechDispatcher[] = "enable-speech-dispatcher";
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// Disables the creation and launch of app shims for platform apps.
const char kDisableAppShims[]               = "disable-app-shims";

// Forcibly disables Lion-style on newer OSes, to allow developers to test the
// older, SnowLeopard-style fullscreen.
const char kDisableSystemFullscreenForTesting[] =
    "disable-system-fullscreen-for-testing";

// Enables a simplified fullscreen UI on Mac.
const char kEnableSimplifiedFullscreen[]    = "enable-simplified-fullscreen";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
const char kRelauncherProcess[]             = "relauncher";

#endif

// Use bubbles for content permissions requests instead of infobars.
const char kEnablePermissionsBubbles[]      = "enable-permissions-bubbles";

#if defined(OS_WIN)
// Fallback to XPS. By default connector uses CDD.
const char kEnableCloudPrintXps[]           = "enable-cloud-print-xps";

// Force-enables the profile shortcut manager. This is needed for tests since
// they use a custom-user-data-dir which disables this.
const char kEnableProfileShortcutManager[]  = "enable-profile-shortcut-manager";

// For the DelegateExecute verb handler to launch Chrome in metro mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceImmersive[]                = "force-immersive";

// For the DelegateExecute verb handler to launch Chrome in desktop mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceDesktop[]                  = "force-desktop";

// Relaunches metro Chrome on Windows 8 and higher using a given shortcut.
const char kRelaunchShortcut[]              = "relaunch-shortcut";

// Requests that Chrome connect to the running Metro viewer process.
const char kViewerConnect[]                 = "viewer-connect";

// Requests that Chrome launch the Metro viewer process via the given appid
// (which is assumed to be registered as default browser) and synchronously
// connect to it.
const char kViewerLaunchViaAppId[]          = "viewer-launch-via-appid";

// Waits for the given handle to be signaled before relaunching metro Chrome on
// Windows 8 and higher.
const char kWaitForMutex[]                  = "wait-for-mutex";

// Indicates that chrome was launched to service a search request in Windows 8.
const char kWindows8Search[]           = "windows8-search";
#endif

#if defined(ENABLE_FULL_PRINTING) && !defined(OFFICIAL_BUILD)
// Enables support to debug printing subsystem.
const char kDebugPrint[] = "debug-print";
#endif

#ifndef NDEBUG
// Enables overriding the path of file manager extension.
const char kFileManagerExtensionPath[]      = "filemgr-ext-path";

// Enables overriding the path of image loader extension.
const char kImageLoaderExtensionPath[]      = "image-loader-ext-path";
#endif  // NDEBUG

// Controls print preview in the browser process.
#if defined(GOOGLE_CHROME_BUILD)
// Disables print preview (For testing, and for users who don't like us. :[ )
const char kDisablePrintPreview[]           = "disable-print-preview";
#else
// Enables print preview. Chromium normally does not have the PDF viewer,
// required for print preview.
// pdf.dll or libpdf.so should be present in primary directory of
// Chromium. For local builds it's usually out/Debug or out/Release.
const char kEnablePrintPreview[]            = "enable-print-preview";
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR CRAP TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead, please
// put them in alphabetical order above, or in order inside the appropriate
// ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
