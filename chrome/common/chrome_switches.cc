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

// Allows choosing an existing managed user profile during the managed
// user creation flow.
const char kAllowCreateExistingManagedUsers[] =
    "allow-create-existing-managed-users";

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// On ChromeOS, file:// access is disabled except for certain whitelisted
// directories. This switch re-enables file:// for testing.
const char kAllowFileAccess[]               = "allow-file-access";

// Allows non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[]       = "allow-http-background-page";

// Allow non-secure origins to use the screen capture API.
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

// Flag to enable apps_devtool app.
const char kAppsDevtool[]                   = "apps-devtool";

// Specifies the initial size for application windows launched with --app.
// --app-window-size=w,h
const char kAppWindowSize[]                 = "app-window-size";

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

// Disable throbber for extension apps.
const char kAppsNoThrob[]                   = "apps-no-throb";

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

// The value of this switch tells the app to listen for and broadcast
// automation-related messages on IPC channel with the given ID.
const char kAutomationClientChannelID[]     = "automation-channel";

// Causes the automation provider to reinitialize its IPC channel instead of
// shutting down when a client disconnects.
const char kAutomationReinitializeOnChannelError[] =
    "automation-reinitialize-on-channel-error";

// Similar to kNoFirstRun, but also drops the First Run beacon so that first run
// will not occur in subsequent runs either.
const char kCancelFirstRun[]                = "cancel-first-run";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Checks the cloud print connector policy, informing the service process if
// the policy is set to disallow the connector, then quits.
const char kCheckCloudPrintConnectorPolicy[] =
    "check-cloud-print-connector-policy";

// Run Chrome in Chrome Frame mode. This means that Chrome expects to be run
// as a dependent process of the Chrome Frame plugin.
const char kChromeFrame[]                   = "chrome-frame";

// Tells chrome to load the specified version of chrome.dll on Windows. If this
// version cannot be loaded, Chrome will exit.
const char kChromeVersion[]                 = "chrome-version";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[]          = "cipher-suite-blacklist";

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[]             = "clear-token-service";

// The maximum amount of delay in ms between receiving a cloud policy
// invalidation and fetching the policy. A random delay up to this value is used
// to prevent Chrome clients from overwhelming the cloud policy server when a
// policy which affects many users is changed.
const char kCloudPolicyInvalidationDelay[]  = "cloud-policy-invalidation-delay";

// Used with kCloudPrintFile. Tells Chrome to delete the file when finished
// displaying the print dialog.
const char kCloudPrintDeleteFile[]          = "cloud-print-delete-file";

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
// enabled (see enable-cloud-print).
const char kCloudPrintServiceURL[]          = "cloud-print-service";

// Comma-separated options to troubleshoot the component updater. Only valid
// for the browser process.
const char kComponentUpdater[]              = "component-updater";

// Causes the browser process to inspect loaded and registered DLLs for known
// conflicts and warn the user.
const char kConflictingModulesCheck[]       = "conflicting-modules-check";

// Toggles a new version of the content settings dialog in options.
const char kContentSettings2[]              = "new-content-settings";

// The Country we should use. This is normally obtained from the operating
// system during first run and cached in the preferences afterwards. This is a
// string value, the 2 letter code from ISO 3166-1.
const char kCountry[]                       = "country";

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

// Enables support to debug printing subsystem.
const char kDebugPrint[]                    = "debug-print";

// Specifies the URL at which to fetch configuration policy from the device
// management backend. Specifying this switch turns on managed policy from the
// device management backend.
const char kDeviceManagementUrl[]           = "device-management-url";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
const char kDiagnosticsFormat[]             = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
const char kDiagnosticsRecovery[]           = "diagnostics-recovery";

// If set, the app list will be disabled at startup. Note this doesn't prevent
// the app list from running, it just makes Chrome think the app list hasn't
// been enabled (as in kEnableAppList) yet.
const char kDisableAppList[]                = "disable-app-list";

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

// Disables more strict popup blocking.
const char kDisableBetterPopupBlocking[]    = "disable-better-popup-blocking";

// Disables the bundled PPAPI version of Flash.
const char kDisableBundledPpapiFlash[]      = "disable-bundled-ppapi-flash";

// Disables the bookmark autocomplete provider (BookmarkProvider).
const char kDisableBookmarkAutocompleteProvider[] =
    "disable-bookmark-autocomplete-provider";

// Disables the client-side phishing detection feature. Note that even if
// client-side phishing detection is enabled, it will only be active if the
// user has opted in to UMA stats and SafeBrowsing is enabled in the
// preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

const char kDisableComponentUpdate[]        = "disable-component-update";

// Disables establishing certificate revocation information by downloading a
// set of CRLs rather than performing on-line checks.
const char kDisableCRLSets[]                = "disable-crl-sets";

// Disables the custom JumpList on Windows 7.
const char kDisableCustomJumpList[]         = "disable-custom-jumplist";

// Disables installation of default apps on first run. This is used during
// automated testing.
const char kDisableDefaultApps[]            = "disable-default-apps";

// Disables retrieval of PAC URLs from DHCP as per the WPAD standard.
const char kDisableDhcpWpad[]               = "disable-dhcp-wpad";

// Force-disables DNS probes on main frame DNS errors.
const char kDisableDnsProbes[]              = "disable-dns-probes";

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

// Disable Instant extended API.
const char kDisableInstantExtendedAPI[]     = "disable-instant-extended-api";

// Disables improved SafeBrowsing download protection.
const char kDisableImprovedDownloadProtection[] =
    "disable-improved-download-protection";

// Don't resolve hostnames to IPv6 addresses. This can be used when debugging
// issues relating to IPv6, but shouldn't otherwise be needed. Be sure to file
// bugs if something isn't working properly in the presence of IPv6. This flag
// can be overidden by the "enable-ipv6" flag.
const char kDisableIPv6[]                   = "disable-ipv6";

// Disables IP Pooling within the networks stack (SPDY only). When a connection
// is needed for a domain which shares an IP with an existing connection,
// attempt to use the existing connection.
const char kDisableIPPooling[]              = "disable-ip-pooling";

// Disable always using the local NTP for the first NTP load of a new window.
const char kDisableLocalFirstLoadNTP[] = "disable-local-first-load-ntp";

// Disable the behavior that the second click on a launcher item (the click when
// the item is already active) minimizes the item.
const char kDisableMinimizeOnSecondLauncherItemClick[] =
    "disable-minimize-on-second-launcher-item-click";

// Disables the menu on the NTP for accessing sessions from other devices.
const char kDisableNTPOtherSessionsMenu[]   = "disable-ntp-other-sessions-menu";

// Disables omnibox auto-completion when IME is active.
const char kDisableOmniboxAutoCompletionForIme[] =
    "disable-omnibox-auto-completion-for-ime";

// Disable using a public suffix based domain matching for autofill of
// passwords.
const char kDisablePasswordAutofillPublicSuffixDomainMatching[] =
    "disable-password-autofill-public-suffix-domain-matching";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

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

// Prevents the URLs of BackgroundContents from being remembered and
// re-launched when the browser restarts.
const char kDisableRestoreBackgroundContents[] =
    "disable-restore-background-contents";

// Disables restoring session state (cookies, session storage, etc.) when
// restoring the browsing session.
const char kDisableRestoreSessionState[]    = "disable-restore-session-state";

// Disables throttling prints initiated by scripts.
const char kDisableScriptedPrintThrottling[] =
    "disable-scripted-print-throttling";

// Disables syncing browser data to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Disables syncing of app settings.
const char kDisableSyncAppSettings[]        = "disable-sync-app-settings";

// Disables syncing of apps.
const char kDisableSyncApps[]               = "disable-sync-apps";

// Disables syncing of autofill.
const char kDisableSyncAutofill[]           = "disable-sync-autofill";

// Disables syncing of autofill Profile.
const char kDisableSyncAutofillProfile[]    = "disable-sync-autofill-profile";

// Disables syncing of bookmarks.
const char kDisableSyncBookmarks[]          = "disable-sync-bookmarks";

// Disables syncing of dictionary.
const char kDisableSyncDictionary[]         = "disable-sync-dictionary";

// Disables syncing extension settings.
const char kDisableSyncExtensionSettings[]  = "disable-sync-extension-settings";

// Disables syncing of extensions.
const char kDisableSyncExtensions[]         = "disable-sync-extensions";

// Disables syncing of favicons.
const char kDisableSyncFavicons[]           = "disable-sync-favicons";

// Disables syncing browser passwords.
const char kDisableSyncPasswords[]          = "disable-sync-passwords";

// Disables syncing of preferences.
const char kDisableSyncPreferences[]        = "disable-sync-preferences";

// Disables syncing of priority preferences.
const char kDisableSyncPriorityPreferences[] =
    "disable-sync-priority-preferences";

// Disable syncing custom search engines.
const char kDisableSyncSearchEngines[]      = "disable-sync-search-engines";

// Disable synced notifications.
const char kDisableSyncSyncedNotifications[] =
    "disable-sync-synced-notifications";

// Disables syncing browser sessions. Will override kEnableSyncTabs.
const char kDisableSyncTabs[]               = "disable-sync-tabs";

// Disables syncing of themes.
const char kDisableSyncThemes[]             = "disable-sync-themes";

// Disables syncing browser typed urls.
const char kDisableSyncTypedUrls[]          = "disable-sync-typed-urls";

// Allows disabling of translate from the command line to assist with automated
// browser testing (e.g. Selenium/WebDriver). Normal browser users should
// disable translate with the preference.
const char kDisableTranslate[]              = "disable-translate";

// Disables TLS Channel ID extension.
const char kDisableTLSChannelID[]           = "disable-tls-channel-id";

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

// Enables the <adview> tag in packaged apps.
const char kEnableAdview[]                  = "enable-adview";

// If set, the app list will be enabled as if enabled from CWS.
const char kEnableAppList[]                 = "enable-app-list";

// Enables specifying a "src" attribute on <adview> elements
// (for testing purposes, to skip the whitelist).
const char kEnableAdviewSrcAttribute[]      = "enable-adview-src-attribute";

// Enables the <window-controls> tag in platform apps.
const char kEnableAppWindowControls[]       = "enable-app-window-controls";

// Enables the experimental asynchronous DNS client.
const char kEnableAsyncDns[]                = "enable-async-dns";

// Enables the inclusion of non-standard ports when generating the Kerberos SPN
// in response to a Negotiate challenge. See
// HttpAuthHandlerNegotiate::CreateSPN for more background.
const char kEnableAuthNegotiatePort[]       = "enable-auth-negotiate-port";

// Enable using a public suffix based domain matching for autofill of passwords.
const char kEnablePasswordAutofillPublicSuffixDomainMatching[] =
    "enable-password-autofill-public-suffix-domain-matching";

// Enables the pre- and auto-login features. When a user signs in to sync, the
// browser's cookie jar is pre-filled with GAIA cookies. When the user visits a
// GAIA login page, an info bar can help the user login.
const char kEnableAutologin[]               = "enable-autologin";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enables pushing cloud policy to Chrome using an invalidation service.
const char kEnableCloudPolicyPush[]         = "enable-cloud-policy-push";

// This applies only when the process type is "service". Enables the Cloud
// Print Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// Enables fetching and storing cloud policy for components. This currently
// supports policy for extensions on Chrome OS.
const char kEnableComponentCloudPolicy[]    = "enable-component-cloud-policy";

// Enables fetching the user's contacts from Google and showing them in the
// Chrome OS apps list.
const char kEnableContacts[]                = "enable-contacts";

// Enables device discovery.
const char kEnableDeviceDiscovery[]        = "enable-device-discovery";

// If true devtools experimental settings are enabled.
const char kEnableDevToolsExperiments[]     = "enable-devtools-experiments";

// Force-enables DNS probes on main frame DNS errors.  (The user must still
// opt in to "Use web service to resolve navigation errors".)
const char kEnableDnsProbes[]               = "enable-dns-probes";

// Enables extensions to be easily installed from sites other than the web
// store. Without this flag, they can still be installed, but must be manually
// dragged onto chrome://extensions/.
const char kEasyOffStoreExtensionInstall[]  =
    "easy-off-store-extension-install";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

const char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Enables or disables showing extensions in the action box.
const char kExtensionsInActionBox[]         = "extensions-in-action-box";

// Enable the fast unload controller, which speeds up tab/window close by
// running a tab's onunload js handler independently of the GUI -
// crbug.com/142458 .
const char kEnableFastUnload[]         = "enable-fast-unload";

// By default, cookies are not allowed on file://. They are needed for testing,
// for example page cycler and layout tests. See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enables Google Now integration.
const char kEnableGoogleNowIntegration[]    = "enable-google-now-integration";

// Enable HTTP/2 draft 04. This is a temporary testing flag.
const char kEnableHttp2Draft04[]            = "enable-http2-draft-04";

// Enable Instant extended API. On mobile, this merely enables query extraction,
// not the rest of the instant-extended functionality.
const char kEnableInstantExtendedAPI[]      = "enable-instant-extended-api";

// Enables IPv6 support, even if probes suggest that it may not be fully
// supported. Some probes may require internet connections, and this flag will
// allow support independent of application testing. This flag overrides
// "disable-ipv6" which appears elswhere in this file.
const char kEnableIPv6[]                    = "enable-ipv6";

/// Enables the IPC fuzzer for reliability testing
const char kEnableIPCFuzzing[]              = "enable-ipc-fuzzing";

// Enables IP Pooling within the networks stack (SPDY only). When a connection
// is needed for a domain which shares an IP with an existing connection,
// attempt to use the existing connection.
const char kEnableIPPooling[]               = "enable-ip-pooling";

// Enable always using the local NTP for the first NTP load of a new window.
const char kEnableLocalFirstLoadNTP[] = "enable-local-first-load-ntp";

// Enables support for user profiles that are managed by another user and can
// have restrictions applied.
const char kEnableManagedUsers[]            = "enable-managed-users";

// Make the values returned to window.performance.memory more granular and more
// up to date. Without this flag, the memory information is still available, but
// it is bucketized and updated less frequently.
const char kEnableMemoryInfo[]              = "enable-memory-info";

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

// Enables NPN and SPDY. In case server supports SPDY, browser will use SPDY.
const char kEnableNpn[]                     = "enable-npn";

// Enables NPN with HTTP. It means NPN is enabled but SPDY won't be used.
// HTTP is still used for all requests.
const char kEnableNpnHttpOnly[]             = "enable-npn-http";

// Enables omnibox auto-completion when IME is active.  The auto-completion for
// IME is shown in the same style as the normal(non-IME) auto-completion.
const char kEnableOmniboxAutoCompletionForIme[] =
    "enable-omnibox-auto-completion-for-ime";

// Enables panels (always on-top docked pop-up windows).
const char kEnablePanels[]                  = "enable-panels";

// Enables password generation when we detect that the user is going through
// account creation.
const char kEnablePasswordGeneration[]      = "enable-password-generation";

// Enables the usage of Portable Native Client.
const char kEnablePnacl[]                   = "enable-pnacl";

// Disables the installation of Portable Native Client.
const char kDisablePnaclInstall[]           = "disable-pnacl-install";

// Enables tracking of tasks in profiler for viewing via about:profiler.
// To predominantly disable tracking (profiling), use the command line switch:
// --enable-profiling=0
// Some tracking will still take place at startup, but it will be turned off
// during chrome_browser_main.
const char kEnableProfiling[]               = "enable-profiling";

// Enables support for the QUIC protocol.  This is a temporary testing flag.
const char kEnableQuic[]                    = "enable-quic";

// Enables support for the HTTPS over QUIC protocol.  This is a temporary
// testing flag.  This only has an effect if QUIC protocol is enabled.
const char kEnableQuicHttps[]               = "enable-quic-https";

// Enables the Quickoffoce/Chrome document viewer rather than the editor.
const char kEnableQuickofficeViewing[]      = "enable-quickoffice-viewing";

// Enables support in chrome://settings to reset settings in your profile
// that are often touched by malware.
const char kEnableResetProfileSettings[]    = "enable-reset-profile-settings";

// Enables content settings based on host *and* plug-in in the user
// preferences.
const char kEnableResourceContentSettings[] =
    "enable-resource-content-settings";

// Enables save password prompt bubble.
const char kEnableSavePasswordBubble[]      = "enable-save-password-bubble";

// Controls the support for SDCH filtering (dictionary based expansion of
// content). By default SDCH filtering is enabled. To disable SDCH filtering,
// use "--enable-sdch=0" as command line argument. SDCH is currently only
// supported server-side for searches on google.com.
const char kEnableSdch[]                    = "enable-sdch";

// Enables support of sticky keys.
const char kEnableStickyKeys[]              = "enable-sticky-keys";

// Disables support of sticky keys.
const char kDisableStickyKeys[]              = "disable-sticky-keys";

// Disable SPDY/3.1. This is a temporary testing flag.
const char kDisableSpdy31[]                 = "disable-spdy31";

// Enable SPDY/4 alpha 2. This is a temporary testing flag.
const char kEnableSpdy4a2[]                 = "enable-spdy4a2";

// Enable SPDY CREDENTIAL frame support.  This is a temporary testing flag.
const char kEnableSpdyCredentialFrames[]    = "enable-spdy-credential-frames";

// Enables auto correction for misspelled words.
const char kEnableSpellingAutoCorrect[]     = "enable-spelling-auto-correct";

// Enables sending user feedback to spelling service.
const char kEnableSpellingServiceFeedback[] =
    "enable-spelling-service-feedback";

// Enables the stacked tabstrip.
const char kEnableStackedTabStrip[]         = "enable-stacked-tab-strip";

// Enables experimental suggestions pane in New Tab page.
const char kEnableSuggestionsTabPage[]      = "enable-suggestions-ntp";

// Enables synced notifications.
const char kEnableSyncSyncedNotifications[] =
    "enable-sync-synced-notifications";

// Enables context menu for selecting groups of tabs.
const char kEnableTabGroupsContextMenu[]    = "enable-tab-groups-context-menu";

// Enables fanciful thumbnail processing. Used with NTP for
// instant-extended-api, where thumbnails are generally smaller.
const char kEnableThumbnailRetargeting[]   = "enable-thumbnail-retargeting";

// Enables Translate settings in chrome://settings/languages.
const char kEnableTranslateSettings[]      = "enable-translate-settings";

// Enables unrestricted SSL 3.0 fallback.
// With this switch, SSL 3.0 fallback will be enabled for all sites.
// Without this switch, SSL 3.0 fallback will be disabled for a site
// pinned to the Google pin list (indicating that it is a Google site).
// Note: until http://crbug/237055 is resolved, unrestricted SSL 3.0
// fallback is always enabled, with or without this switch.
const char kEnableUnrestrictedSSL3Fallback[] =
    "enable-unrestricted-ssl3-fallback";

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

// The time in seconds that an extension event page can be idle before it
// is shut down.
const char kEventPageIdleTime[]             = "event-page-idle-time";

// The time in seconds that an extension event page has between being notified
// of its impending unload and that unload happening.
const char kEventPageSuspendingTime[]       = "event-page-unloading-time";

// Marks a renderer as extension process.
const char kExtensionProcess[]              = "extension-process";

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
// whether or not it's actually the First Run (this overrides kNoFirstRun and
// kCancelFirstRun).
const char kForceFirstRun[]                 = "force-first-run";

// Tries to load cloud policy for every signed in user, regardless of whether
// they are a dasher user or not. Used to allow any GAIA account to be used for
// testing the cloud policy framework.
const char kForceLoadCloudPolicy[]          = "force-load-cloud-policy";

// Enables using GAIA information to populate profile name and icon.
const char kGaiaProfileInfo[]               = "gaia-profile-info";

// Specifies an alternate URL to use for speaking to Google. Useful for testing.
const char kGoogleBaseURL[]                 = "google-base-url";

// Specifies an alternate URL to use for retrieving the search domain for
// Google. Useful for testing.
const char kGoogleSearchDomainCheckURL[]    = "google-search-domain-check-url";

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

// Disables full history sync.
const char kHistoryDisableFullHistorySync[] = "disable-full-history-sync";

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

// Causes Chrome to attempt to get metadata from the webstore for the
// app/extension ID given, and then prompt the user to download and install it.
const char kInstallFromWebstore[]           = "install-from-webstore";

// Causes Chrome to load this URL instead of chrome://newtab for New Tab pages.
const char kInstantNewTabURL[]              = "instant-new-tab-url";

// Marks a renderer as an Instant process.
const char kInstantProcess[]                = "instant-process";

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

// If present, disables the loading and application of cloud policy for
// signed-in users.
const char kDisableCloudPolicyOnSignin[]    = "disable-cloud-policy-on-signin";

// Loads an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Makes Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Used to authenticate requests to the Sync service for managed users. Setting
// this switch also causes Sync to be set up for a managed user.
const char kManagedUserSyncToken[]          = "managed-user-sync-token";

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

// List of native messaging hosts outside of the default location. Used for
// tests. The value must be comma-separate lists of key-value pairs separated
// equal sign. E.g. "host1=/path/to/host1/manifest.json,host2=/path/host2.json".
const char kNativeMessagingHosts[]          = "native-messaging-hosts";

// Sets the base logging level for the net log. Log 0 logs the most data.
// Intended primarily for use with --log-net-log.
const char kNetLogLevel[]                   = "net-log-level";

// Use new profile management system, including profile sign-out and new
// choosers.
const char kNewProfileManagement[]    = "new-profile-management";

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
// by kForceFirstRun.
// Also drops the First Run beacon so that First Run will not occur in
// subsequent runs as well.
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

// When the option to block third-party cookies is enabled, only block
// third-party cookies from being set.
const char kOnlyBlockSettingThirdPartyCookies[] =
    "only-block-setting-third-party-cookies";

// Launches URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Simulates an organic Chrome install.
const char kOrganicInstall[]                = "organic";

// Force use of QUIC for requests to the specified origin.
const char kOriginToForceQuicOn[]           = "origin-to-force-quic-on";

// The time that a new chrome process which is delegating to an already running
// chrome process started. (See ProcessSingleton for more details.)
const char kOriginalProcessStartTime[]      = "original-process-start-time";

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

// Overrides the path to the location that PNaCl is installed.
const char kPnaclDir[]                      = "pnacl-dir";

// Forces the PPAPI version of Flash (if it's being used) to run in the
// renderer process rather than in a separate plugin process.
const char kPpapiFlashInProcess[]           = "ppapi-flash-in-process";

// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[]                = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[]             = "ppapi-flash-version";

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
//   auto: Allow field trial selection in both prerender and prefetch.
const char kPrerenderModeSwitchValueAuto[]  = "auto";
//   disabled: No prerendering or prefetching.
const char kPrerenderModeSwitchValueDisabled[] = "disabled";
//   enabled: Both prerendering and prefetching.
const char kPrerenderModeSwitchValueEnabled[] = "enabled";
//   prefetch_only: No prerendering, but enables prefetching.
const char kPrerenderModeSwitchValuePrefetchOnly[] = "prefetch_only";

// Enable conversion from vector to raster for any page.
const char kPrintRaster[]                   = "print-raster";

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

// Should we prompt the user before allowing external extensions to install?
// Default is yes.
const char kPromptForExternalExtensions[]   = "prompt-for-external-extensions";

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

// Adds a "Purge memory" button to the Task Manager, which tries to dump as
// much memory as possible. This is mostly useful for testing how well the
// MemoryPurger functionality works.
//
// NOTE: This is only implemented for Views.
const char kPurgeMemoryButton[]             = "purge-memory-button";

// Capture resource consumption information through page cycling and output the
// data to the specified file.
const char kRecordStats[]                   = "record-stats";

// Chrome supports a playback and record mode.  Record mode saves *everything*
// to the cache.  Playback mode reads data exclusively from the cache.  This
// allows us to record a session into the cache and then replay it at will.
// See also kPlaybackMode.
const char kRecordMode[]                    = "record-mode";

// Uses custom front-end URL for the remote debugging.
const char kRemoteDebuggingFrontend[]       = "remote-debugging-frontend";

// Enables remote debugging of Chrome for Android via raw USB.
const char kRemoteDebuggingRawUSB[]       = "remote-debugging-raw-usb";

// Enables print preview in the renderer. This flag is generated internally by
// Chrome and does nothing when directly passed to the browser.
const char kRendererPrintPreview[]          = "renderer-print-preview";

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

// URL prefix used by safebrowsing to fetch hash, download data and report
// malware.
const char kSbURLPrefix[]                   = "safebrowsing-url-prefix";

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

// URL to send safebrowsing download feedback reports to.
const char kSbDownloadFeedbackURL[] = "safebrowsing-download-feedback-url";

// Enable safebrowsing download feedback.
const char kSbEnableDownloadFeedback[] =
    "safebrowsing-enable-download-feedback";

// Enables or disables extension scripts badges in the location bar.
const char kScriptBadges[]                  = "script-badges";

// Enable or diable the "script bubble" icon in the URL bar that tells you how
// many extensions are running scripts on a page.
const char kScriptBubble[]                  = "script-bubble";

// Causes the process to run as a service process.
const char kServiceProcess[]                = "service";

// Sets a token in the token service, for testing.
const char kSetToken[]                      = "set-token";

// If true the app list will be shown.
const char kShowAppList[]                   = "show-app-list";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// If true the alignment of the shelf can be changed.
const char kShowShelfAlignmentMenu[]        = "show-launcher-alignment-menu";

// Marks a renderer as the signin process.
const char kSigninProcess[]                 = "signin-process";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
const char kSilentDebuggerExtensionAPI[]    = "silent-debugger-extension-api";

// Changes the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when --enable-dcheck is
// specified.
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

// Replaces the buffered data source for <audio> and <video> with a simplified
// resource loader that downloads the entire resource into memory.

// Origin for which SpdyProxy authentication is supported.
const char kSpdyProxyAuthOrigin[]           = "spdy-proxy-auth-origin";

// Speculative resource prefetching.
const char kSpeculativeResourcePrefetching[] =
    "speculative-resource-prefetching";

// Speculative resource prefetching is disabled.
const char kSpeculativeResourcePrefetchingDisabled[] = "disabled";

// Speculative resource prefetching will only learn about resources that need to
// be prefetched but will not prefetch them.
const char kSpeculativeResourcePrefetchingLearning[] = "learning";

// Speculative resource prefetching is enabled.
const char kSpeculativeResourcePrefetchingEnabled[] = "enabled";

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
const char kSyncEnableDeferredStartup[]     = "sync-enable-deferred-startup";

// Disables use of OAuth2 token in sync components and reverts behavior to
// ClientLogin token.
const char kSyncDisableOAuth2Token[]         = "sync-disable-oauth2-token";

// Enables feature to avoid unnecessary GetUpdate requests.
const char kSyncEnableGetUpdateAvoidance[]   =
    "sync-enable-get-update-avoidance";

// Enables directory support for sync filesystem
const char kSyncfsEnableDirectoryOperation[] =
    "enable-syncfs-directory-operation";

// Enables tab dragging to create a real browser.
const char kTabBrowserDragging[]            = "enable-tab-browser-dragging";

// Passes the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Type of the current test harness ("browser" or "ui").
const char kTestType[]                      = "test-type";

// Tells the app to listen for and broadcast testing-related messages on IPC
// channel with the given ID.
const char kTestingChannelID[]              = "testing-channel";

// Enables tracking the amount of non-idle time spent viewing pages.
const char kTrackActiveVisitTime[]          = "track-active-visit-time";

// Overrides the default server used for Google Translate.
const char kTranslateScriptURL[]            = "translate-script-url";

// Overrides security-origin with which Translate runs in an isolated world.
const char kTranslateSecurityOrigin[]       = "translate-security-origin";

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

// Sets the maximum concurrent streams over a SPDY session.
const char kMaxSpdyConcurrentStreams[]      = "max-spdy-concurrent-streams";

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

// Cycle through a series of URLs listed in the specified file.
const char kVisitURLs[]                     = "visit-urls";

// Adds the given extension ID to all the permission whitelists.
const char kWhitelistedExtensionID[]        = "whitelisted-extension-id";

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

#if defined(OS_ANDROID) || defined(OS_IOS)
// Enable SPDY proxy.
const char kEnableSpdyProxyAuth[]           = "enable-spdy-proxy-auth";
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

#if defined(OS_ANDROID)
// Enables the new NTP.
const char kEnableNewNTP[]                  = "enable-new-ntp";

// Pops the translate infobar if possible.
const char kEnableTranslate[]               = "enable-translate";

// Registers for cloud policy using the BROWSER client type instead of the
// ANDROID_BROWSER type. This enables skipping the server whitelist.
// TODO(joaodasilva): remove this. http://crbug.com/248527
const char kFakeCloudPolicyType[]           = "fake-cloud-policy-type";

// Uses the tablet specific UI components when available.
const char kTabletUI[]                      = "tablet-ui";
#endif

#if defined(USE_ASH)
const char kAshDisableTabScrubbing[]        = "ash-disable-tab-scrubbing";
const char kOpenAsh[]                       = "open-ash";
#endif

#if defined(OS_POSIX)
// Used for turning on Breakpad crash reporting in a debug environment where
// crash reporting is typically compiled but disabled.
const char kEnableCrashReporterForTesting[] =
    "enable-crash-reporter-for-testing";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// Specifies which password store to use (detect, default, gnome, kwallet).
const char kPasswordStore[]                 = "password-store";
#endif
#endif  // OS_POSIX

#if defined(OS_MACOSX)
// Forcibly disables Lion-style on newer OSes, to allow developers to test the
// older, SnowLeopard-style fullscreen.
const char kDisableSystemFullscreenForTesting[] =
    "disable-system-fullscreen-for-testing";

// Enables the app list OSX .app shim, for showing the app list. If the flag is
// not present, Chrome will check if the shim exists at startup, and delete it
// if it does.
const char kEnableAppListShim[]             = "enable-app-list-shim";

// Enable to allow creation and launch of app shims for platform apps.
const char kEnableAppShims[]                = "enable-app-shims";

// Enables the tabs expose feature ( http://crbug.com/50307 ).
const char kEnableExposeForTabs[]           = "enable-expose-for-tabs";

// Enables a simplified fullscreen UI on Mac.
const char kEnableSimplifiedFullscreen[]    = "enable-simplified-fullscreen";

// Performs Keychain reauthorization from the command line on behalf of a
// special Keychain reauthorization stub executable. Used during auto-update.
const char kKeychainReauthorize[]           = "keychain-reauthorize";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
const char kRelauncherProcess[]             = "relauncher";

// Uses mock keychain for testing purposes, which prevents blocking dialogs
// from causing timeouts.
const char kUseMockKeychain[]               = "use-mock-keychain";
#endif

#if defined(OS_WIN)
// For the DelegateExecute verb handler to launch Chrome in metro mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceImmersive[]                = "force-immersive";

// For the DelegateExecute verb handler to launch Chrome in desktop mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceDesktop[]                  = "force-desktop";

// Allows for disabling the overlapped I/O for TCP reads.
// Possible values are "on" or "off".
// The default is "on" which matches the existing behavior.
// "off" switches to use non-blocking reads and WSAEventSelect.
const char kOverlappedRead[]                = "overlapped-reads";

// Relaunches metro Chrome on Windows 8 and higher using a given shortcut.
const char kRelaunchShortcut[]              = "relaunch-shortcut";

// Waits for the given handle to be signaled before relaunching metro Chrome on
// Windows 8 and higher.
const char kWaitForMutex[]                  = "wait-for-mutex";

// Indicates that chrome was launched to service a search request in Windows 8.
const char kWindows8Search[]           = "windows8-search";

#endif

#if defined(OS_WIN) && defined(USE_AURA)
// Requests that Chrome connect to the running Metro viewer process.
const char kViewerConnect[]                 = "viewer-connect";

// Requests that Chrome launch the Metro viewer process via the given appid
// (which is assumed to be registered as default browser) and synchronously
// connect to it.
const char kViewerLaunchViaAppId[]          = "viewer-launch-via-appid";
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
// Enables print preview (Force enable on Chromium, which normally does not
//                        have the PDF viewer required for print preview.)
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
