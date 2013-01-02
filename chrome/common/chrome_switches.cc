// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.
//
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Enables or disables the "action box" UI in the toolbar.
const char kActionBox[]                     = "action-box";

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// On ChromeOS, file:// access is disabled except for certain whitelisted
// directories. This switch re-enables file:// for testing.
const char kAllowFileAccess[]               = "allow-file-access";

// Allows non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[]       = "allow-http-background-page";

// Allows the browser to load extensions that lack a modern manifest when that
// would otherwise be forbidden.
const char kAllowLegacyExtensionManifests[] =
    "allow-legacy-extension-manifests";

// Specifies comma-separated list of extension ids to grant access to TCP/UDP
// socket APIs.
const char kAllowNaClSocketAPI[]            = "allow-nacl-socket-api";

// Don't block outdated plugins.
const char kAllowOutdatedPlugins[]          = "allow-outdated-plugins";

// By default, an https page cannot run JavaScript, CSS or plug-ins from http
// URLs. This provides an override to get the old insecure behavior.
const char kAllowRunningInsecureContent[]   = "allow-running-insecure-content";

// Allows injecting extensions and user scripts on the extensions gallery
// site. Normally prevented for security reasons, but can be useful for
// automation testing of the gallery.
const char kAllowScriptingGallery[]         = "allow-scripting-gallery";

// Prevents Chrome from requiring authorization to run certain widely installed
// but less commonly used plug-ins.
const char kAlwaysAuthorizePlugins[]        = "always-authorize-plugins";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Specifies that the associated value should be launched in "application"
// mode.
const char kApp[]                           = "app";

// Flag to enable apps_debugger app.
const char kAppsDebugger[]                  = "apps-debugger";

// Specifies the initial size for application windows launched with --app.
// --app-window-size=w,h
const char kAppWindowSize[]                 = "app-window-size";

// A URL for the server which assigns channel ids for server pushed app
// notifications.
const char kAppNotifyChannelServerURL[]     = "app-notify-channel-server-url";

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

// Allows the webstorePrivate APIs to return browser (aka sync) login tokens to
// be used for auto-login in the Web Store (normally they do not).
const char kAppsGalleryReturnTokens[]       = "apps-gallery-return-tokens";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[]                = "apps-gallery-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[]          = "apps-gallery-update-url";

// Whether to always use the new app install bubble when installing an app.
const char kAppsNewInstallBubble[]          = "apps-new-install-bubble";

// Disable throbber for extension apps.
const char kAppsNoThrob[]                   = "apps-no-throb";

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

// Flag used to tell Chrome the base url of the Autofill service.
const char kAutofillServiceUrl[]            = "autofill-service-url";

// The value of this switch tells the app to listen for and broadcast
// automation-related messages on IPC channel with the given ID.
const char kAutomationClientChannelID[]     = "automation-channel";

// Causes the automation provider to reinitialize its IPC channel instead of
// shutting down when a client disconnects.
const char kAutomationReinitializeOnChannelError[] =
    "automation-reinitialize-on-channel-error";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Checks the cloud print connector policy, informing the service process if
// the policy is set to disallow the connector, then quits.
const char kCheckCloudPrintConnectorPolicy[] =
    "check-cloud-print-connector-policy";

// Tells Chrome to delay shutdown (for a specified number of seconds) when a
// Chrome Frame automation channel is closed.
const char kChromeFrameShutdownDelay[]      = "chrome-frame-shutdown-delay";

// Tells chrome to load the specified version of chrome.dll on Windows. If this
// version cannot be loaded, Chrome will exit.
const char kChromeVersion[]                 = "chrome-version";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[]          = "cipher-suite-blacklist";

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[]             = "clear-token-service";

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

// The unique id to be used for this cloud print proxy instance.
const char kCloudPrintProxyId[]             = "cloud-print-proxy-id";

// The URL of the cloud print service to use, overrides any value stored in
// preferences, and the default. Only used if the cloud print service has been
// enabled (see enable-cloud-print).
const char kCloudPrintServiceURL[]          = "cloud-print-service";

// Comma-separated options to troubleshoot the component updater. Only valid
// for the browser process.
const char kComponentUpdaterDebug[]         = "component-updater-debug";

// Causes the browser process to inspect loaded and registered DLLs for known
// conflicts and warn the user.
const char kConflictingModulesCheck[]       = "conflicting-modules-check";

// Toggles a new version of the content settings dialog in options.
const char kContentSettings2[]              = "new-content-settings";

// The Country we should use. This is normally obtained from the operating
// system during first run and cached in the preferences afterwards. This is a
// string value, the 2 letter code from ISO 3166-1.
const char kCountry[]                       = "country";

// Causes the browser process to crash if browser threads are not responding
// for the given number of seconds.
const char kCrashOnHangSeconds[]            = "crash-on-hang-seconds";

// Comma-separated list of BrowserThreads that cause browser process to crash
// if the given browser thread is not responsive. UI,IO,DB,FILE,CACHE are the
// list of BrowserThreads that are supported.
//
// For example:
//    --crash-on-hang-threads=UI:3,IO:3 --> Crash the browser if UI or IO thread
//                                          is not responsive and the number of
//                                          browser threads that are responding
//                                          is less than or equal to 3.
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
const char kDebugPackedApps[]        = "debug-packed-apps";

// Enables support to debug printing subsystem.
const char kDebugPrint[]                    = "debug-print";

// Specifies the URL at which to fetch configuration policy from the device
// management backend. Specifying this switch turns on managed policy from the
// device management backend.
const char kDeviceManagementUrl[]           = "device-management-url";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Replaces the audio IPC layer for <audio> and <video> with a mock audio
// device, useful when using remote desktop or machines without sound cards.
// This is temporary until we fix the underlying problem.

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

// Disables the bookmark autocomplete provider (BookmarkProvider).
const char kDisableBookmarkAutocompleteProvider[] =
    "disable-bookmark-autocomplete-provider";

// Disables the client-side phishing detection feature. Note that even if
// client-side phishing detection is enabled, it will only be active if the
// user has opted in to UMA stats and SafeBrowsing is enabled in the
// preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

// Disables the new cloud policy stack.
const char kDisableCloudPolicyService[]     = "disable-cloud-policy-service";

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

// Disables the menu on the NTP for accessing sessions from other devices.
const char kDisableNTPOtherSessionsMenu[]   = "disable-ntp-other-sessions-menu";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Disable speculative TCP/IP preconnection.
const char kDisablePreconnect[]             = "disable-preconnect";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

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

// Disable syncing app notifications.
const char kDisableSyncAppNotifications[]   = "disable-sync-app-notifications";

// Disables syncing of autofill.
const char kDisableSyncAutofill[]           = "disable-sync-autofill";

// Disables syncing of autofill Profile.
const char kDisableSyncAutofillProfile[]    = "disable-sync-autofill-profile";

// Disables syncing of bookmarks.
const char kDisableSyncBookmarks[]          = "disable-sync-bookmarks";

// Disables syncing extension settings.
const char kDisableSyncExtensionSettings[]  = "disable-sync-extension-settings";

// Disables syncing of extensions.
const char kDisableSyncExtensions[]         = "disable-sync-extensions";

// Disables syncing of history delete directives.
const char kDisableSyncHistoryDeleteDirectives[] =
    "disable-sync-history-delete-directives";

// Disables syncing browser passwords.
const char kDisableSyncPasswords[]          = "disable-sync-passwords";

// Disables syncing of preferences.
const char kDisableSyncPreferences[]        = "disable-sync-preferences";

// Disable syncing custom search engines.
const char kDisableSyncSearchEngines[]      = "disable-sync-search-engines";

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

// Disables the website settings UI.
const char kDisableWebsiteSettings[]         = "disable-website-settings";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/119175)
extern const char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Disables prefetching of DNS information.
const char kDnsPrefetchDisable[]            = "dns-prefetch-disable";

// Dump any accumualted histograms to the log when browser terminates (requires
// logging to be enabled to really do anything). Used by developers and test
// scripts.
const char kDumpHistogramsOnExit[]          = "dump-histograms-on-exit";

// Enables the opt-in UI for the app list.
const char kEnableAppListOptIn[]            = "enable-app-list-opt-in";

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

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// This applies only when the process type is "service". Enables the Cloud
// Print Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// Enables fetching the user's contacts from Google and showing them in the
// Chrome OS apps list.
const char kEnableContacts[]                = "enable-contacts";

// Enables web developers to create apps for Chrome without using crx packages.
const char kEnableCrxlessWebApps[]          = "enable-crxless-web-apps";

// Enables desktop guest mode.
const char kEnableDesktopGuestMode[]        = "enable-desktop-guest-mode";

// If true devtools experimental settings are enabled.
const char kEnableDevToolsExperiments[]     = "enable-devtools-experiments";

// Enables an interactive autocomplete UI and a way to invoke this UI from
// WebKit by enabling HTMLFormElement#requestAutocomplete (and associated
// autocomplete* events and logic).
const char kEnableInteractiveAutocomplete[] = "enable-interactive-autocomplete";

// Enables extensions to be easily installed from sites other than the web
// store. Without this flag, they can still be installed, but must be manually
// dragged onto chrome://extensions/.
const char kEasyOffStoreExtensionInstall[] = "easy-off-store-extension-install";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enable autofill for new elements like checkboxes. crbug.com/157636
const char kEnableExperimentalFormFilling[] =
    "enable-experimental-form-filling";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

// Enables the extension activity UI.
const char kEnableExtensionActivityUI[]     = "enable-extension-activity-ui";

// Enables or disables showing extensions in the action box.
const char kExtensionsInActionBox[]         = "extensions-in-action-box";

// By default, cookies are not allowed on file://. They are needed for testing,
// for example page cycler and layout tests. See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enables Google Now integration.
const char kEnableGoogleNowIntegration[] = "enable-google-now-integration";

// Enable HTTP pipelining. Attempt to pipeline HTTP connections. Heuristics will
// try to figure out if pipelining can be used for a given host and request.
// Without this flag, pipelining will never be used.
const char kEnableHttpPipelining[]          = "enable-http-pipelining";

// Enable Instant extended API.
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

// Enables support for user profiles that are managed by another user and can
// have restrictions applied.
extern const char kEnableManagedUsers[]     = "enable-managed-users";

// Allows reporting memory info (JS heap size) to page.
const char kEnableMemoryInfo[]              = "enable-memory-info";

// Enables metrics recording and reporting in the browser startup sequence, as
// if this was an official Chrome build where the user allowed metrics
// reporting. This is used for testing only.
const char kEnableMetricsReportingForTesting[] =
    "enable-metrics-reporting-for-testing";

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enables debugging via RSP over a socket.
const char kEnableNaClDebug[]               = "enable-nacl-debug";

// Uses NaCl manifest URL to choose whether NaCl program will be debugged by
// debug stub.
// Switch value format: [!]pattern1,pattern2,...,patternN. Each pattern uses
// the same syntax as patterns in Chrome extension manifest. The only difference
// is that * scheme matches all schemes instead of matching only http and https.
// If the value doesn't start with !, a program will be debugged if manifest URL
// matches any pattern. If the value starts with !, a program will be debugged
// if manifest URL does not match any pattern.
const char kNaClDebugMask[]                 = "nacl-debug-mask";

// Enables the SRPC Proxy for NaCl. The default is the Chrome IPC based  proxy.
// TODO(bbudge) remove this after we switch to IPC and remove SRPC.
const char kEnableNaClSRPCProxy[]           = "enable-nacl-srpc-proxy";

// Enables hardware exception handling via debugger process.
const char kEnableNaClExceptionHandling[]   = "enable-nacl-exception-handling";

// Enables the native messaging extensions API.
const char kEnableNativeMessaging[]         = "enable-native-messaging";

// Enables the new Autofill UI, which is part of the browser process rather than
// part of the renderer process.  http://crbug.com/51644
const char kEnableNewAutofillUi[]           = "enable-new-autofill-ui";

// Enables new Autofill heuristics, such as adding support for new field types.
const char kEnableNewAutofillHeuristics[]   = "enable-new-autofill-heuristics";

// Enables NPN and SPDY. In case server supports SPDY, browser will use SPDY.
const char kEnableNpn[]                     = "enable-npn";

// Enables NPN with HTTP. It means NPN is enabled but SPDY won't be used.
// HTTP is still used for all requests.
const char kEnableNpnHttpOnly[]             = "enable-npn-http";

// Enables panels (always on-top docked pop-up windows).
const char kEnablePanels[]                  = "enable-panels";

// Enables panel stacking support.
const char kEnablePanelStacking[]           = "enable-panel-stacking";

// Enables password generation when we detect that the user is going through
// account creation.
const char kEnablePasswordGeneration[]      = "enable-password-generation";

// Enables content settings based on host *and* plug-in in the user
// preferences.
const char kEnableResourceContentSettings[] =
    "enable-resource-content-settings";

// Enables rich templated notifications and NotificationCenter.
const char kEnableRichNotifications[]       = "enable-rich-notifications";

// Enables the installation and usage of Portable Native Client.
const char kEnablePnacl[]                   = "enable-pnacl";

// Enables tracking of tasks in profiler for viewing via about:profiler.
// To predominantly disable tracking (profiling), use the command line switch:
// --enable-profiling=0
// Some tracking will still take place at startup, but it will be turned off
// during chrome_browser_main.
const char kEnableProfiling[]               = "enable-profiling";


// Controls the support for SDCH filtering (dictionary based expansion of
// content). By default SDCH filtering is enabled. To disable SDCH filtering,
// use "--enable-sdch=0" as command line argument. SDCH is currently only
// supported server-side for searches on google.com.
const char kEnableSdch[]                    = "enable-sdch";

// Enable SPDY/3. This is a temporary testing flag.
const char kEnableSpdy3[]                   = "enable-spdy3";

// Enable SPDY CREDENTIAL frame support.  This is a temporary testing flag.
const char kEnableSpdyCredentialFrames[]    = "enable-spdy-credential-frames";

// Enables auto correction for misspelled words.
const char kEnableSpellingAutoCorrect[]    = "enable-spelling-auto-correct";

// Enables the stacked tabstrip.
const char kEnableStackedTabStrip[]         = "enable-stacked-tab-strip";

// Enables experimental suggestions pane in New Tab page.
const char kEnableSuggestionsTabPage[]      = "enable-suggestions-ntp";

// Enables context menu for selecting groups of tabs.
const char kEnableTabGroupsContextMenu[]    = "enable-tab-groups-context-menu";

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
const char kEventPageUnloadingTime[]        = "event-page-unloading-time";

// Marks a renderer as extension process.
const char kExtensionProcess[]              = "extension-process";

// Frequency in seconds for Extensions auto-update.
const char kExtensionsUpdateFrequency[]     = "extensions-update-frequency";

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

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
const char kForceFirstRun[]                      = "force-first-run";

// Enables using GAIA information to populate profile name and icon.
const char kGaiaProfileInfo[]               = "gaia-profile-info";

// Specifies an alternate URL to use for retrieving the search domain for
// Google. Useful for testing.
const char kGoogleSearchDomainCheckURL[] = "google-search-domain-check-url";

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

// Enables full history sync (not just typed URLs) for signed-in users.
const char kHistoryEnableFullHistorySync[]       = "enable-full-history-sync";

// Disables full history sync.
const char kHistoryDisableFullHistorySync[]      = "disable-full-history-sync";

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

// Takes the JSON-formatted HSTS specification and loads it as if it were a
// preloaded HSTS entry. Takes precedence over both website-specified rules and
// built-in rules. The JSON format is the same as that persisted in
// <profile_dir>/Default/TransportSecurity
const char kHstsHosts[]                     = "hsts-hosts";

// Performs importing from another browser. The value associated with this
// setting encodes the target browser and what items to import.
const char kImport[]                        = "import";

// Performs bookmark importing from an HTML file. The value associated with
// this setting encodes the file path. It may be used jointly with kImport.
const char kImportFromFile[]                = "import-from-file";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Causes Chrome to attempt to get metadata from the webstore for the
// app/extension ID given, and then prompt the user to download and install it.
const char kInstallFromWebstore[]    = "install-from-webstore";

// URL to use for instant. If specified this overrides the url from the
// TemplateURL.
const char kInstantURL[]                    = "instant-url";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[]              = "keep-alive-for-test";

// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";

// Print automatically in kiosk mode. |kKioskMode| must be set as well.
// See http://crbug.com/31395.
const char kKioskModePrinting[]             = "kiosk-printing";

// Comma-separated list of directories with component extensions to load.
const char kLoadComponentExtension[]        = "load-component-extension";

// If present, cloud policy will be loaded and applied once the user is signed
// in to the browser.
const char kLoadCloudPolicyOnSignin[]        = "load-cloud-policy-on-signin";

// Loads an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Loads the opencryptoki library into NSS at startup. This is only needed
// temporarily for developers who need to work on WiFi/VPN certificate code.
//
// TODO(gspencer): Remove this switch once cryptohomed work is finished:
// http://crosbug.com/12295 and http://crosbug.com/12304
const char kLoadOpencryptoki[]              = "load-opencryptoki";

// Enables displaying net log events on the command line, or writing the events
// to a separate file if a file name is given.
const char kLogNetLog[]                     = "log-net-log";

// Uninstalls an extension with the specified extension id.
const char kUninstallExtension[]            = "uninstall-extension";

// Starts the browser in managed mode.
const char kManaged[]                       = "managed";

// Makes Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

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

// Native Client GDB debugger for loader. It needs switches calculated
// at run time in order to work correctly. That's why NaClLoadCmdPrefix
// flag can't be used.
const char kNaClGdb[]                       = "nacl-gdb";

// GDB script to pass to the nacl-gdb debugger at startup.
const char kNaClGdbScript[]                 = "nacl-gdb-script";

// On POSIX only: the contents of this flag are prepended to the nacl-loader
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kNaClLoaderCmdPrefix[]           = "nacl-loader-cmd-prefix";

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

// Starts the browser outside of managed mode.
const char kNoManaged[]                     = "no-managed";

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

// Shows a desktop notification that the cloud print token has expired and that
// user needs to re-authenticate.
const char kNotifyCloudPrintTokenExpired[]  = "notify-cp-token-expired";

// Specifies the maximum number of threads to use for running the Proxy
// Autoconfig (PAC) script.
const char kNumPacThreads[]                 = "num-pac-threads";

// Controls whether to use the fancy new scoring (takes into account
// word breaks, does better balancing of topicality, recency, etc.) for
// HistoryQuickProvider.
const char kOmniboxHistoryQuickProviderNewScoring[] =
    "omnibox-history-quick-provider-new-scoring";
// The value the kOmniboxHistoryQuickProviderNewScoring switch may have,
// as in "--omnibox-history-quick-provider-new-scoring=1".
// 1 means use new scoring.
const char kOmniboxHistoryQuickProviderNewScoringEnabled[] = "1";
// 0 means use old scoring ( == current behavior as of 6/2012).
const char kOmniboxHistoryQuickProviderNewScoringDisabled[] = "0";

// Controls whether HistoryQuickProvider is allowed to reorder results
// according to inlineability in order to more aggressively assign/keep
// high relevance scores.
const char kOmniboxHistoryQuickProviderReorderForInlining[] =
    "omnibox-history-quick-provider-reorder-for-inlining";
// The value the kOmniboxHistoryQuickProviderReorderForInlining switch may
// have, as in "--omnibox-history-quick-provider-reorder-for-inlining=1".
// 1 means allow reordering results.
const char kOmniboxHistoryQuickProviderReorderForInliningEnabled[] = "1";
// 0 means don't allow reordering results ( == current behavior as of 6/2012).
const char kOmniboxHistoryQuickProviderReorderForInliningDisabled[] = "0";

// Controls whether the omnibox's HistoryQuickProvider is allowed to
// inline suggestions.
const char kOmniboxInlineHistoryQuickProvider[] =
    "omnibox-inline-history-quick-provider-allowed";
// The values the kOmniboxInlineHistoryQuickProvider switch may have, as in
// "--omnibox-inline-history-quick-provider-allowed=1"
//   allowed: if HistoryQuickProvider thinks it appropriate, it can inline
//   ( == current behavior as of 2/2012).
const char kOmniboxInlineHistoryQuickProviderAllowed[] = "1";
//   prohibited: never inline matches
const char kOmniboxInlineHistoryQuickProviderProhibited[] = "0";
//   auto: any other value => the code and field trial does what it wants.

// When the option to block third-party cookies is enabled, only block
// third-party cookies from being set.
const char kOnlyBlockSettingThirdPartyCookies[] =
    "only-block-setting-third-party-cookies";

// Launches URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Simulates an organic Chrome install.
const char kOrganicInstall[]                = "organic";

// Force use of QUIC for requests over the specified port.
const char kOriginPortToForceQuicOn[]    = "origin-port-to-force-quic-on";

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
const char kPerformanceMonitorGathering[]  = "performance-monitor-gathering";

// Enable the post crash analyzer which uploads detailed crash information in
// situations where a crash is determined to be particularly interesting.
const char kPerformCrashAnalysis[]          = "perform-crash-analysis";

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
const char kPrintRaster[]              = "print-raster";

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

// Enables or disables extension scripts badges in the location bar.
const char kScriptBadges[]                  = "script-badges";

// Enable or diable the "script bubble" icon in the URL bar that tells you how
// many extensions are running scripts on a page.
const char kScriptBubble[]                  = "script-bubble";

// Enables the showing of an info-bar instructing user they can search directly
// from the omnibox.
const char kSearchInOmniboxHint[]           = "search-in-omnibox-hint";

// Sets a token in the token service, for testing.
const char kSetToken[]                      = "set-token";

// If true the app list will be shown.
const char kShowAppList[]                   = "show-app-list";

// If true an app list shortcut will be shown in the taskbar.
const char kShowAppListShortcut[]           = "show-app-list-shortcut";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[]   = "show-autofill-type-predictions";

// Makes component extensions appear in chrome://settings/extensions.
const char kShowComponentExtensionOptions[] =
    "show-component-extension-options";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// If true the alignment of the launcher can be changed.
const char kShowLauncherAlignmentMenu[]     = "show-launcher-alignment-menu";

// Enables or disables sideload wipeout extension effort.
const char kSideloadWipeout[]               = "sideload-wipeout";

// Changes the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when --enable-dcheck is
// specified.
const char kSilentDumpOnDCHECK[]            = "silent-dump-on-dcheck";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
const char kSilentLaunch[]                  = "silent-launch";

// Simulates an update being available.
const char kSimulateUpgrade[]               = "simulate-upgrade";

// Replaces the buffered data source for <audio> and <video> with a simplified
// resource loader that downloads the entire resource into memory.

// Socket reuse policy. The value should be of type enum
// ClientSocketReusePolicy.
const char kSocketReusePolicy[]             = "socket-reuse-policy";

// Origin for which SpdyProxy authentication is supported.
const char kSpdyProxyOrigin[]               = "spdy-proxy-origin";

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

// Enable support for keystore key based encryption.
const char kSyncKeystoreEncryption[] = "sync-keystore-encryption";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// Overrides the default notification method for sync.
const char kSyncNotificationMethod[]        = "sync-notification-method";

// Overrides the default host:port used for sync notifications.
const char kSyncNotificationHostPort[]      = "sync-notification-host-port";

// Overrides the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Enables syncing of favicons as part of tab sync.
const char kSyncTabFavicons[]                = "sync-tab-favicons";

// Makes the sync code to throw an unrecoverable error after initialization.
// Useful for testing unrecoverable error scenarios.
const char kSyncThrowUnrecoverableError[]   = "sync-throw-unrecoverable-error";


// Tries to connect to XMPP using SSLTCP first (for testing).
const char kSyncTrySsltcpFirstForXmpp[]     = "sync-try-ssltcp-first-for-xmpp";

// Enables tab dragging to create a real browser.
const char kTabBrowserDragging[]            = "enable-tab-browser-dragging";

// Enables tab capture.
const char kTabCapture[]                    = "enable-tab-capture";

// Passes the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Runs the security test for the NaCl loader sandbox.
const char kTestNaClSandbox[]               = "test-nacl-sandbox";

// Type of the current test harness ("browser" or "ui").
const char kTestType[]                      = "test-type";

// Tells the app to listen for and broadcast testing-related messages on IPC
// channel with the given ID.
const char kTestingChannelID[]              = "testing-channel";

// Disables same-origin check on HTTP resources pushed via a SPDY proxy.
// The value is the host:port of the trusted proxy.
const char kTrustedSpdyProxy[] = "trusted-spdy-proxy";

// Experimental. Shows a dialog asking the user to try chrome. This flag is to
// be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[] = "use-gpu-in-tests";

// Uses Spdy for the transport protocol instead of HTTP. This is a temporary
// testing flag.
const char kUseSpdy[]                       = "use-spdy";

// Enables use of the spelling web service. This will only work if asynchronous
// spell checking is not disabled.
const char kUseSpellingService[]            = "use-spelling-service";

// Sets the maximum SPDY sessions per domain.
const char kMaxSpdySessionsPerDomain[]      = "max-spdy-sessions-per-domain";

// Sets the maximum concurrent streams over a SPDY session.
const char kMaxSpdyConcurrentStreams[]      = "max-spdy-concurrent-streams";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// Uses the GAIA web-based signin flow instead of the native UI signin flow.
const char kUseWebBasedSigninFlow[]         = "use-web-based-signin-flow";

// Specifies a custom URL for the server which reports variation data to the
// client. See variations_service.cc.
const char kVariationsServerURL[]            = "variations-server-url";

// Prints version information and quits.
const char kVersion[]                       = "version";

// Requests that Chrome connect to a remote viewer process using an IPC
// channel of the given name.
const char kViewerConnection[]              = "viewer-connection";

// Cycle through a series of URLs listed in the specified file.
const char kVisitURLs[]                     = "visit-urls";

// Secure service URL for Online Wallet service. Used for encyption of one time
// pads and to escrow credit card numbers.
const char kWalletSecureServiceUrl[]        = "wallet-secure-service-url";

// Service URL for Online Wallet service. Used as the base url for Online Wallet
// API calls.
const char kWalletServiceUrl[]              = "wallet-service-url";

// Enable the "native services" feature of web-intents.
const char kWebIntentsNativeServicesEnabled[] =
    "web-intents-native-services-enabled";

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

#if defined(OS_ANDROID)
// Use the tablet specific UI components when available.
const char kTabletUI[]                      = "tablet-ui";
#endif

#if defined(USE_ASH)
const char kAshEnableTabScrubbing[]            = "ash-enable-tab-scrubbing";
#endif

#if defined(OS_CHROMEOS)
// When wallpaper boot animation is not disabled this switch
// is used to override OOBE/sign in WebUI init type.
// Possible values: parallel|postpone. Default: parallel.
const char kAshWebUIInit[]                  = "ash-webui-init";

// Enables switching between different cellular carriers from the UI.
const char kEnableCarrierSwitching[]        = "enable-carrier-switching";

// Disables wallpaper boot animation (except of OOBE case).
const char kDisableBootAnimation[]          = "disable-boot-animation";

// Disables Chrome Captive Portal detector, which initiates Captive
// Portal detection for new active networks.
const char kDisableChromeCaptivePortalDetector[] =
    "disable-chrome-captive-portal-detector";

// Disables Google Drive integration.
const char kDisableDrive[]                  = "disable-drive";

// Disables file prefetching in Google Drive Client for Chrome OS.
const char kDisableDrivePrefetch[]          = "disable-drive-prefetch";

// Avoid doing expensive animations upon login.
const char kDisableLoginAnimations[]        = "disable-login-animations";

// Disables new OOBE/sign in design.
const char kDisableNewOobe[]                = "disable-new-oobe";

// Disables new password changed dialog (WebUI).
const char kDisableNewPasswordChangedDialog[] =
    "disable-new-password-changed-dialog";

// Avoid doing animations upon oobe.
const char kDisableOobeAnimation[]          = "disable-oobe-animation";

// Disables fake ethernet network on the login screen.
const char kDisableStubEthernet[]           = "disable-stub-ethernet";

// Enables component extension that initializes background pages of
// certain hosted applications.
const char kEnableBackgroundLoader[]        = "enable-background-loader";

// Enables Chrome Captive Portal detector, which initiates Captive
// Portal detection for new active networks.
const char kEnableChromeCaptivePortalDetector[] =
    "enable-chrome-captive-portal-detector";

// Enables touchpad three-finger-click as middle button.
const char kEnableTouchpadThreeFingerClick[]
    = "enable-touchpad-three-finger-click";

// Enables touchpad three-finger swipe.
const char kEnableTouchpadThreeFingerSwipe[]
    = "enable-touchpad-three-finger-swipe";

// Skips OAuth part of ChromeOS login process.
const char kSkipOAuthLogin[]                = "skip-oauth-login";

// Enable Kiosk mode for ChromeOS.
const char kEnableKioskMode[]               = "enable-kiosk-mode";

// Disable policy-configured local accounts.
const char kDisableLocalAccounts[]          = "disable-local-accounts";

// Enables request of tablet site (via user agent override).
const char kEnableRequestTabletSite[]       = "enable-request-tablet-site";

// Enables static ip configuration. This flag should be removed when it's on by
// default.
const char kEnableStaticIPConfig[]          = "enable-static-ip-config";

// Passed to Chrome on first boot. Not passed on restart after sign out.
const char kFirstBoot[] = "first-boot";

// If true, the Chromebook has a Chrome OS keyboard. Don't use the flag for
// Chromeboxes.
const char kHasChromeOSKeyboard[]           = "has-chromeos-keyboard";

// Path for the screensaver used in Kiosk mode
const char kKioskModeScreensaverPath[]      = "kiosk-mode-screensaver-path";

// Enables Chrome-as-a-login-manager behavior.
const char kLoginManager[]                  = "login-manager";

// Allows to override the first login screen. The value should be the name of
// the first login screen to show (see
// chrome/browser/chromeos/login/login_wizard_view.cc for actual names).
// Ignored if kLoginManager is not specified. TODO(avayvod): Remove when the
// switch is no longer needed for testing.
const char kLoginScreen[]                   = "login-screen";

// Controls the initial login screen size. Pass width,height.
const char kLoginScreenSize[]               = "login-screen-size";

// Specifies the profile to use once a chromeos user is logged in.
const char kLoginProfile[]                  = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[]                     = "login-user";

// Specifies a password to be used to login (along with login-user).
const char kLoginPassword[]                 = "login-password";

// Enables natural scroll by default.
const char kNaturalScrollDefault[]          = "enable-natural-scroll-default";

// Disables tab discard in low memory conditions, a feature which silently
// closes inactive tabs to free memory and to attempt to avoid the kernel's
// out-of-memory process killer.
const char kNoDiscardTabs[]                 = "no-discard-tabs";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[]                  = "bwsi";

// Enables overriding the path for the default echo component extension.
// Useful for testing.
const char kEchoExtensionPath[]             = "echo-ext-path";

// Indicates that a stub implementation of CrosSettings that stores settings in
// memory without signing should be used, treating current user as the owner.
// This option is for testing the chromeos build of chrome on the desktop only.
const char kStubCrosSettings[]              = "stub-cros-settings";

// Enables overriding the path for the default authentication extension.
const char kAuthExtensionPath[]             = "auth-ext-path";

// Power of the power-of-2 initial modulus that will be used by the
// auto-enrollment client. E.g. "4" means the modulus will be 2^4 = 16.
const char kEnterpriseEnrollmentInitialModulus[] =
    "enterprise-enrollment-initial-modulus";

// Power of the power-of-2 maximum modulus that will be used by the
// auto-enrollment client.
const char kEnterpriseEnrollmentModulusLimit[] =
    "enterprise-enrollment-modulus-limit";

// Loads the File Manager as a packaged app.
const char kFileManagerPackaged[] = "file-manager-packaged";

#ifndef NDEBUG
// Skips all other OOBE pages after user login.
const char kOobeSkipPostLogin[]             = "oobe-skip-postlogin";
#endif  // NDEBUG
#endif  // OS_CHROMEOS

#if defined(OS_POSIX)
// A flag, generated internally by Chrome for renderer and other helper process
// command lines on Linux and Mac. It tells the helper process to enable crash
// dumping and reporting, because helpers cannot access the profile or other
// files needed to make this decision.
const char kEnableCrashReporter[]           = "enable-crash-reporter";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// Specifies which password store to use (detect, default, gnome, kwallet).
const char kPasswordStore[]                 = "password-store";
#endif
#endif  // OS_POSIX

#if defined(OS_MACOSX)
// Enables the tabs expose feature ( http://crbug.com/50307 ).
const char kEnableExposeForTabs[]           = "enable-expose-for-tabs";

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
// Disables profile desktop shortcuts handling, preventing their creation,
// modification or removal.
const char kDisableDesktopShortcuts[]       = "disable-desktop-shortcuts";

// For the DelegateExecute verb handler to launch Chrome in metro mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceImmersive[]                 = "force-immersive";

// For the DelegateExecute verb handler to launch Chrome in desktop mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceDesktop[]                   = "force-desktop";

// Allows for disabling the overlapped I/O for TCP reads.
// Possible values are "on" or "off".
// The default is "on" which matches the existing behavior.
// "off" switches to use non-blocking reads and WSAEventSelect.
const char kOverlappedRead[]                 = "overlapped-reads";

// Relaunches metro Chrome on Windows 8 and higher using a given shortcut.
const char kRelaunchShortcut[]               = "relaunch-shortcut";

// Waits for the given handle to be signaled before relaunching metro Chrome on
// Windows 8 and higher.
const char kWaitForMutex[]                  = "wait-for-mutex";
#endif

#ifndef NDEBUG
// Enables overriding the path of file manager extension.
const char kFileManagerExtensionPath[]      = "filemgr-ext-path";

// Dumps dependency information about our profile services into a dot file in
// the profile directory.
const char kDumpProfileDependencyGraph[]    = "dump-profile-graph";
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
