// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.
// -----------------------------------------------------------------------------

// Activate (make foreground) myself on launch.  Helpful when Chrome
// is launched on the command line (e.g. by Selenium).  Only needed on Mac.
const char kActivateOnLaunch[]              = "activate-on-launch";

// Allow third party content included on a page to prompt for a HTTP
// basic auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// On ChromeOS, file:// access is disabled except for certain whitelisted
// directories. This switch re-enables file:// for testing.
const char kAllowFileAccess[]               = "allow-file-access";

// Allow non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[]       = "allow-http-background-page";

// Don't block outdated plugins.
const char kAllowOutdatedPlugins[]          = "allow-outdated-plugins";

// Allows injecting extensions and user scripts on the extensions
// gallery site. Normally prevented for security reasons, but can be
// useful for automation testing of the gallery.
const char kAllowScriptingGallery[]         = "allow-scripting-gallery";

// Specifies comma separated list of extension ids to grant access to local
// websocket proxy.
const char kAllowWebSocketProxy[]           = "allow-websocket-proxy";

// This prevents Chrome from requiring authorization to run certain widely
// installed but less commonly used plug-ins.
const char kAlwaysAuthorizePlugins[]        = "always-authorize-plugins";

// Enable web inspector for all windows, even if they're part of the browser.
// Allows us to use our dev tools to debug browser windows itself.
const char kAlwaysEnableDevTools[]          = "always-enable-dev-tools";

// Specifies that the associated value should be launched in "application" mode.
const char kApp[]                           = "app";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Override the apps checkout URL, which is used to determine when to expose
// some private APIs.
const char kAppsCheckoutURL[]               = "apps-checkout-url";

// Specifying this flag allows the webstorePrivate APIs to return browser (aka
// sync) login tokens to be used for auto-login in the Web Store (normally they
// do not).
const char kAppsGalleryReturnTokens[]       = "apps-gallery-return-tokens";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[]                = "apps-gallery-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[]          = "apps-gallery-update-url";

// Disable throbber for extension apps.
const char kAppsNoThrob[]                   = "apps-no-throb";

// Whitelist of servers that Negotiate will generate delegated Kerberos tickets
// for.
const char kAuthNegotiateDelegateWhitelist[] =
    "auth-negotiate-delegate-whitelist";

// HTTP authentication schemes to enable. This is a comma separated list
// of authentication schemes (basic, digest, ntlm, and negotiate). By default
// all schemes are enabled. The primary use of this command line flag is to help
// triage autentication-related issues reported by end-users.
const char kAuthSchemes[]                   = "auth-schemes";

// Whitelist of servers which NTLM and Negotiate can automatically authenticate
// with using the default credentials of the currently logged in user.
const char kAuthServerWhitelist[]           = "auth-server-whitelist";

// The value of this switch tells the app to listen for and broadcast
// automation-related messages on IPC channel with the given ID.
const char kAutomationClientChannelID[]     = "automation-channel";

// When the option to block third-party cookies from being set is enabled,
// also block third-party cookies from being read.
const char kBlockReadingThirdPartyCookies[] =
    "block-reading-third-party-cookies";

// Causes the browser process to throw an assertion on startup.
const char kBrowserAssertTest[]             = "assert-test";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// How often (in seconds) to check for updates. Should only be used for
// testing purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Run Chrome in Chrome Frame mode. This means that Chrome expects to be run
// as a dependent process of the Chrome Frame plugin.
const char kChromeFrame[]                   = "chrome-frame";

// Tells chrome to load the specified version of chrome.dll on Windows. If
// this version cannot be loaded, Chrome will exit.
const char kChromeVersion[]                 = "chrome-version";

// Used with kCloudPrintFile.  Tells Chrome to delete the file when
// finished displaying the print dialog.
const char kCloudPrintDeleteFile[]          = "cloud-print-delete-file";

// Tells chrome to display the cloud print dialog and upload the
// specified file for printing.
const char kCloudPrintFile[]                = "cloud-print-file";

// Specifies the mime type to be used when uploading data from the
// file referenced by cloud-print-file.
// Defaults to "application/pdf" if unspecified.
const char kCloudPrintFileType[]            = "cloud-print-file-type";

// Used with kCloudPrintFile to specify a title for the resulting print
// job.
const char kCloudPrintJobTitle[]            = "cloud-print-job-title";

// The unique id to be used for this cloud print proxy instance.
const char kCloudPrintProxyId[]             = "cloud-print-proxy-id";

// The URL of the cloud print service to use, overrides any value
// stored in preferences, and the default.  Only used if the cloud
// print service has been enabled (see enable-cloud-print).
const char kCloudPrintServiceURL[]          = "cloud-print-service";

// Causes the browser process to inspect loaded and registered DLLs for
// known conflicts and warn the user.
const char kConflictingModulesCheck[]       = "conflicting-modules-check";

// The Country we should use.  This is normally obtained from the operating
// system during first run and cached in the preferences afterwards.  This is a
// string value, the 2 letter code from ISO 3166-1.
const char kCountry[]                       = "country";

// Path to the inspector files on disk (allows reloading of devtool files
// without having to restart the browser).
const char kDebugDevToolsFrontend[]         = "debug-devtools-frontend";

// Enables a frame context menu item that toggles the frame in and out of glass
// mode (Windows Vista and up only).
const char kDebugEnableFrameToggle[]        = "debug-enable-frame-toggle";

// Enables support to debug printing subsystem.
const char kDebugPrint[]                    = "debug-print";

// Specifies the URL at which to fetch configuration policy from the device
// management backend. Specifying this switch turns on managed policy from the
// device management backend.
const char kDeviceManagementUrl[]           = "device-management-url";

// Triggers a pletora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Disables the hardware acceleration of 3D CSS and animation.
const char kDisableAcceleratedLayers[]      = "disable-accelerated-layers";

// Disables GPU accelerated video display.
const char kDisableAcceleratedVideo[]       = "disable-accelerated-video";

// Disables the alternate window station for the renderer.
const char kDisableAltWinstation[]          = "disable-winsta";

// Replaces the audio IPC layer for <audio> and <video> with a mock audio
// device, useful when using remote desktop or machines without sound cards.
// This is temporary until we fix the underlying problem.

// Disable CNAME lookup of the host when generating the Kerberos SPN for a
// Negotiate challenge. See HttpAuthHandlerNegotiate::CreateSPN
// for more background.
const char kDisableAuthNegotiateCnameLookup[] =
    "disable-auth-negotiate-cname-lookup";

// Disable background mode (background apps will not keep chrome running in the
// background).
const char kDisableBackgroundMode[] = "disable-background-mode";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise
// in the measurements.
const char kDisableBackgroundNetworking[] = "disable-background-networking";

// This switch is used to disable the client-side phishing detection feature.
// Note that even if client-side phishing detection is enabled, it will only
// be active if the user has opted in to UMA stats and SafeBrowsing is enabled
// in the preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

// Disables establishing a backup TCP connection if a specified timeout is
// exceeded.
const char kDisableConnectBackupJobs[]      = "disable-connect-backup-jobs";

// Disables the custom JumpList on Windows 7.
const char kDisableCustomJumpList[]         = "disable-custom-jumplist";

// Browser flag to disable the web inspector for all renderers.
const char kDisableDevTools[]               = "disable-dev-tools";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";

// Disables the sandbox for the built-in flash player.
const char kDisableFlashSandbox[]           = "disable-flash-sandbox";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the use of the HistoryQuickProvider for autocomplete results.
const char kDisableHistoryQuickProvider[]   = "disable-history-quick-provider";

// Disable the use of the HistoryURLProvider for autocomplete results.
const char kDisableHistoryURLProvider[]     = "disable-history-url-provider";

// Disables HTML5 Forms interactive validation.
const char kDisableInteractiveFormValidation[] =
    "disable-interactive-form-validation";

// Disable the internal Flash Player.
const char kDisableInternalFlash[]          = "disable-internal-flash";

// Don't resolve hostnames to IPv6 addresses. This can be used when debugging
// issues relating to IPv6, but shouldn't otherwise be needed. Be sure to
// file bugs if something isn't working properly in the presence of IPv6.
// This flag can be overidden by the "enable-ipv6" flag.
const char kDisableIPv6[]                   = "disable-ipv6";

// Disables IP Pooling within the networks stack (SPDY only).  When a connection
// is needed for a domain which shares an IP with an existing connection,
// attempt to use the existing connection.
const char kDisableIPPooling[]              = "disable-ip-pooling";

// Disable speculative TCP/IP preconnection.
const char kDisablePreconnect[]             = "disable-preconnect";

// Whether we should prevent the new tab page from showing the first run
// notification.
const char kDisableNewTabFirstRun[]         = "disable-new-tab-first-run";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disable remote web font support. SVG font should always work whether
// this option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Prevents the URLs of BackgroundContents from being remembered and re-launched
// when the browser restarts.
const char kDisableRestoreBackgroundContents[] =
    "disable-restore-background-contents";

// Disable site-specific tailoring to compatibility issues in WebKit.
const char kDisableSiteSpecificQuirks[]     = "disable-site-specific-quirks";

// Disable False Start in SSL and TLS connections.
const char kDisableSSLFalseStart[]          = "disable-ssl-false-start";

// Disable syncing browser data to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Disable syncing of apps.
const char kDisableSyncApps[]               = "disable-sync-apps";

// Disable syncing of autofill.
const char kDisableSyncAutofill[]           = "disable-sync-autofill";

// Disable syncing of autofill Profile.
const char kDisableSyncAutofillProfile[]     = "disable-sync-autofill-profile";

// Disable syncing of bookmarks.
const char kDisableSyncBookmarks[]          = "disable-sync-bookmarks";

// Disable syncing of extensions.
const char kDisableSyncExtensions[]         = "disable-sync-extensions";

// Disable syncing browser passwords.
const char kDisableSyncPasswords[]          = "disable-sync-passwords";

// Disable syncing of preferences.
const char kDisableSyncPreferences[]        = "disable-sync-preferences";

// Disable syncing of themes.
const char kDisableSyncThemes[]             = "disable-sync-themes";

// TabCloseableStateWatcher disallows closing of tabs and browsers under certain
// situations on ChromeOS.  Some tests expect tabs or browsers to close, so we
// need a switch to disable the watcher.
const char kDisableTabCloseableStateWatcher[] =
    "disable-tab-closeable-state-watcher";

// Allow disabling of translate from the command line to assist with
// automated browser testing (e.g. Selenium/WebDriver).  Normal
// browser users should disable translate with the preference.
const char kDisableTranslate[] = "disable-translate";

// Disables the backend service for web resources.
const char kDisableWebResources[]           = "disable-web-resources";

// Don't enforce the same-origin policy.  (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disable WebKit's XSSAuditor.  The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Disables prefetching of DNS information.
const char kDnsPrefetchDisable[]            = "dns-prefetch-disable";

// Use the specified DNS server for raw DNS resolution.
const char kDnsServer[]                     = "dns-server";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating
// dom based tests.
// Also enables sending/receiving renderer automation messages through the
// |AutomationRenderViewHelper|.
// TODO(kkania): Rename this to enable-renderer-automation after moving the
// |DOMAutomationController| to the |AutomationRenderViewHelper|.
const char kDomAutomationController[]       = "dom-automation";

// Dump any accumualted histograms to the log when browser terminates (requires
// logging to be enabled to really do anything).  Used by developers and test
// scripts.
const char kDumpHistogramsOnExit[]          = "dump-histograms-on-exit";

// Enable gpu-accelerated 2d canvas.
const char kEnableAccelerated2dCanvas[]     = "enable-accelerated-2d-canvas";

// Enables the hardware acceleration of plugins.
const char kEnableAcceleratedPlugins[]      = "enable-accelerated-plugins";

// Enables AeroPeek for each tab. (This switch only works on Windows 7).
const char kEnableAeroPeekTabs[]            = "enable-aero-peek-tabs";

// Enable the inclusion of non-standard ports when generating the Kerberos SPN
// in response to a Negotiate challenge. See HttpAuthHandlerNegotiate::CreateSPN
// for more background.
const char kEnableAuthNegotiatePort[]       = "enable-auth-negotiate-port";

// At this point, even if client-side phishing detection is enabled we will not,
// by default, display an interstitial if we detected a phishing site.  Once
// we are confident that the false-positive rate is as low as expected we can
// remove this flag.
const char kEnableClientSidePhishingInterstitial[] =
    "enable-client-side-phishing-interstitial";

// This flag enables UI for clearing server data.  Temporarily in place
// until there's a server endpoint deployed.
const char kEnableClearServerData[]         = "enable-clear-server-data";

// Enable click-to-play for blocked plug-ins.
const char kEnableClickToPlay[]             = "enable-click-to-play";

// This applies only when the process type is "service". Enables the
// Cloud Print Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// Enables the Cloud Print dialog hosting code.
const char kEnableCloudPrint[]              = "enable-cloud-print";

// Enables compact navigation mode, which removes the toolbar and moves most of
// UI to the tab strip.
const char kEnableCompactNavigation[]       = "enable-compact-navigation";

// Enables compositing to texture instead of display.
const char kEnableCompositeToTexture[]      = "enable-composite-to-texture";

// Enables establishing a backup TCP connection if a specified timeout is
// exceeded.
const char kEnableConnectBackupJobs[]       = "enable-connect-backup-jobs";

// Enables web developers to create apps for Chrome without using crx packages.
const char kEnableCrxlessWebApps[]          = "enable-crxless-web-apps";

// Enables retrieval of PAC URLs from DHCP as per the WPAD standard. Note
// that this feature is not supported on all platforms, and using the flag
// is a no-op on such platforms.
const char kEnableDhcpWpad[]                = "enable-dhcp-wpad";

// Enable DNS side checking of certificates. Still experimental, should only
// be used by developers at the current time.
const char kEnableDNSCertProvenanceChecking[] =
     "enable-dns-cert-provenance-checking";

const char kEnableDNSSECCerts[]             = "enable-dnssec-certs";

// Enables app manifest features that are in development.
const char kEnableExperimentalAppManifests[] =
    "enable-experimental-app-manifests";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enable experimental timeline API.
const char kEnableExtensionTimelineApi[]    = "enable-extension-timeline-api";

// Enable the fastback page cache.
const char kEnableFastback[]                = "enable-fastback";

// By default, cookies are not allowed on file://. They are needed for
// testing, for example page cycler and layout tests.  See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enable the JavaScript Full Screen API.
const char kEnableFullScreen[]              = "enable-fullscreen";

// Enable the in-browser thumbnailing, which is more efficient than the
// in-renderer thumbnailing, as we can use more information to determine
// if we need to update thumbnails.
const char kEnableInBrowserThumbnailing[]   = "enable-in-browser-thumbnailing";

// Enable IPv6 support, even if probes suggest that it may not be fully
// supported.  Some probes may require internet connections, and this flag will
// allow support independent of application testing.
// This flag overrides "disable-ipv6" which appears elswhere in this file.
const char kEnableIPv6[]                    = "enable-ipv6";

/// Enable the IPC fuzzer for reliability testing
const char kEnableIPCFuzzing[]               = "enable-ipc-fuzzing";

// Enables IP Pooling within the networks stack (SPDY only).  When a connection
// is needed for a domain which shares an IP with an existing connection,
// attempt to use the existing connection.
const char kEnableIPPooling[]               = "enable-ip-pooling";

// Enables MAC cookies in the network stack.  These cookies use HMAC to
// protect session state from passive network attackers.
// http://tools.ietf.org/html/draft-hammer-oauth-v2-mac-token
const char kEnableMacCookies[]              = "enable-mac-cookies";

// Allows reporting memory info (JS heap size) to page.
const char kEnableMemoryInfo[]              = "enable-memory-info";

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enables debugging via RSP over a socket.
const char kEnableNaClDebug[]               = "enable-nacl-debug";

// This applies only when the process type is "service". Enables the
// Chromoting Host Process within the service process.
const char kEnableRemoting[]                = "enable-remoting";

// Enable content settings based on host *and* plug-in.
const char kEnableResourceContentSettings[] =
    "enable-resource-content-settings";

// Enable panels (always on-top docked pop-up windows).
const char kEnablePanels[]                  = "enable-panels";

// Enable speculative TCP/IP preconnection.
const char kEnablePreconnect[]              = "enable-preconnect";

// Enable the IsSearchProviderInstalled and InstallSearchProvider with an extra
// parameter to indicate if the provider should be the default.
const char kEnableSearchProviderApiV2[]     = "enable-search-provider-api-v2";

// Enables 0-RTT HTTPS handshakes.
const char kEnableSnapStart[]               = "enable-snap-start";

// Enable syncing browser data to a Google Account.
const char kEnableSync[]                    = "enable-sync";

// Enable syncing browser autofill.
const char kEnableSyncAutofill[]            = "enable-sync-autofill";

// Enable syncing browser sessions.
const char kEnableSyncSessions[]            = "enable-sync-sessions";

// Enables context menu for selecting groups of tabs.
const char kEnableTabGroupsContextMenu[]     = "enable-tab-groups-context-menu";

// Enable syncing browser typed urls.
const char kEnableSyncTypedUrls[]           = "enable-sync-typed-urls";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Enables the option to show tabs as a vertical stack down the side of the
// browser window.
const char kEnableVerticalTabs[]            = "enable-vertical-tabs";

// Spawn threads to watch for excessive delays in specified message loops.
// User should set breakpoints on Alarm() to examine problematic thread.
// Usage:   -enable-watchdog=[ui][io]
// Order of the listed sub-arguments does not matter.
const char kEnableWatchdog[]                = "enable-watchdog";

// Enables experimental features for Spellchecker. Right now, the first
// experimental feature is auto spell correct, which corrects words which are
// misppelled by typing the word with two consecutive letters swapped. The
// features that will be added next are:
// 1 - Allow multiple spellcheckers to work simultaneously.
// 2 - Allow automatic detection of spell check language.
// TODO(sidchat): Implement the above fetaures to work under this flag.
const char kExperimentalSpellcheckerFeatures[] =
    "experimental-spellchecker-features";

// Explicitly allow additional ports using a comma separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

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
// OS hard limits. Useful for testing that file descriptor exhaustion is handled
// gracefully.
const char kFileDescriptorLimit[]           = "file-descriptor-limit";

// If true opening a url from the omnibox attepts to focus an existing tab.
const char kFocusExistingTabOnOpen[]        = "focus-existing-tab-on-open";

// Display the First Run experience when the browser is started, regardless of
// whether or not it's actually the first run.
const char kFirstRun[]                      = "first-run";

// Forces the apps/webstore promo to be shown, independent of whether it has
// timed out, etc. Useful for testing.
const char kForceAppsPromoVisible[]         = "force-apps-promo-visible";

// If accelerated compositing is supported, always enter compositing mode for
// the base layer even when compositing is not strictly required.
const char kForceCompositingMode[]          = "force-compositing-mode";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// Specifies a custom name for the GSSAPI library to load.
const char kGSSAPILibraryName[]             = "gssapi-library-name";

// These flags show the man page on Linux. They are equivalent to each
// other.
const char kHelp[]                          = "help";
const char kHelpShort[]                     = "h";

// Make Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This
// only shows an error box because the only way to hide Chrome is by
// uninstalling it.
const char kHideIcons[]                     = "hide-icons";

// The value of this switch specifies which page will be displayed
// in newly-opened tabs.  We need this for testing purposes so
// that the UI tests don't depend on what comes up for http://google.com.
const char kHomePage[]                      = "homepage";

// Comma separated list of rules that control how hostnames are mapped.
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

// These mappings only apply to the host resolver.
const char kHostResolverRules[]             = "host-resolver-rules";

// This switch will take the JSON-formatted HSTS specification and load it
// as if it were a preloaded HSTS entry. It will take precedence over both
// website-specified rules and built-in rules.
// The JSON format is the same as that persisted in
// <profile_dir>/Default/TransportSecurity
const char kHstsHosts[]                     = "hsts-hosts";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[]            = "ignore-gpu-blacklist";

// Perform importing from another browser. The value associated with this
// setting encodes the target browser and what items to import.
const char kImport[]                        = "import";

// Perform bookmark importing from an HTML file. The value associated with this
// setting encodes the file path. It may be used jointly with kImport.
const char kImportFromFile[]                = "import-from-file";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// URL to use for instant. If specified this overrides the url from the
// TemplateURL.
const char kInstantURL[]                    = "instant-url";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[]              = "keep-alive-for-test";

// Load an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Load the opencryptoki library into NSS at startup.  This is only
// needed temporarily for developers who need to work on WiFi/VPN
// certificate code.
// TODO(gspencer): Remove this switch once cryptohomed work is finished:
// http://crosbug.com/12295 and http://crosbug.com/12304
const char kLoadOpencryptoki[]              = "load-opencryptoki";

// Enable displaying net log events on the command line, or writing the events
// to a separate file if a file name is given.
const char kLogNetLog[]                     = "log-net-log";

// Uninstall an extension with the specified extension id.
const char kUninstallExtension[]            = "uninstall-extension";

// Make Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Forces the maximum disk space to be used by the media cache, in bytes.
const char kMediaCacheSize[]                = "media-cache-size";

// Enable dynamic loading of the Memory Profiler DLL, which will trace
// all memory allocations during the run.
const char kMemoryProfiling[]               = "memory-profile";

// Enable histograming of tasks served by MessageLoop. See about:histograms/Loop
// for results, which show frequency of messages on each thread, including APC
// count, object signalling count, etc.
const char kMessageLoopHistogrammer[]       = "message-loop-histogrammer";

// Enables the recording of metrics reports but disables reporting.  In
// contrast to kDisableMetrics, this executes all the code that a normal client
// would use for reporting, except the report is dropped rather than sent to
// the server. This is useful for finding issues in the metrics code during UI
// and performance tests.
const char kMetricsRecordingOnly[]          = "metrics-recording-only";

// The minimum version of Flash that implements the NPP_ClearSiteData API.
const char kMinClearSiteDataFlashVersion[]  = "min-clearsitedata-flash-version";

// Enables multiprofile Chrome.
const char kMultiProfiles[]                 = "multi-profiles";

// Sets the default IP address (interface) for the stub (normally 127.0.0.1).
const char kNaClDebugIP[]                   = "nacl-debug-ip";

// Sets the default port range for debugging.
const char kNaClDebugPorts[]                = "nacl-debug-ports";

// Causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// On POSIX only: the contents of this flag are prepended to the nacl-loader
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kNaClLoaderCmdPrefix[]           = "nacl-loader-cmd-prefix";

// Causes the Native Client process to display a dialog on launch.
const char kNaClStartupDialog[]             = "nacl-startup-dialog";

// Sets the base logging level for the net log.  Log 0 logs the most data.
// Intended primarily for use with --log-net-log.
const char kNetLogLevel[]                   = "net-log-level";

// Use the latest incarnation of the new tab page.
const char kNewTabPage4[]                   = "new-tab-page-4";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// Don't record/playback events when using record & playback.
const char kNoEvents[]                      = "no-events";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
const char kNoExperiments[]                 = "no-experiments";

// whether or not it's actually the first run. Overrides kFirstRun in case
// you're for some reason tempted to pass them both.
const char kNoFirstRun[]                    = "no-first-run";

// Don't send hyperlink auditing pings
const char kNoPings[]                       = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Disables the service process from adding itself as an autorun process. This
// does not delete existing autorun registrations, it just prevents the service
// from registering a new one.
const char kNoServiceAutorun[]               = "no-service-autorun";

// Does not automatically open a browser window on startup (used when launching
// Chrome for the purpose of hosting background apps).
const char kNoStartupWindow[]               = "no-startup-window";

// Show a desktop notification that the cloud print token has expired and
// that user needs to re-authenticate.
const char kNotifyCloudPrintTokenExpired[]  = "notify-cp-token-expired";

// Specifies the maximum number of threads to use for running the Proxy
// Autoconfig (PAC) script.
const char kNumPacThreads[]                 = "num-pac-threads";

// Launch URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Simulate an organic Chrome install.
const char kOrganicInstall[]                = "organic";

// Package an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key is to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Forces the PPAPI version of Flash (if it's being used) to run in the
// renderer process rather than in a separate plugin process.
const char kPpapiFlashInProcess[]          = "ppapi-flash-in-process";

// Controls speculative prerendering of pages, and content prefetching.  Both
// are dispatched from <link rel=prefetch href=...> elements.
const char kPrerender[]                     = "prerender";
// These are the values the switch may have, as in "--prerender=auto".
//   auto: Allow field trial selection in both prerender and prefetch.
const char kPrerenderSwitchValueAuto[]      = "auto";
//   disabled: No prerendering or prefetching.
const char kPrerenderSwitchValueDisabled[]  = "disabled";
//   enabled: Both prerendering and prefetching.
const char kPrerenderSwitchValueEnabled[]   = "enabled";
//   prefetch_only: No prerendering, but enable prefetching.
const char kPrerenderSwitchValuePrefetchOnly[] = "prefetch_only";

// Prints the pages on the screen.
const char kPrint[]                         = "print";

// Output the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
const char kProductVersion[]                = "product-version";

// Starts the sampling based profiler for the browser process at
// startup. This will only work if chrome has been built with
// the gyp variable profiling=1. The output will go to the value
// of kProfilingFile.
const char kProfilingAtStart[]              = "profiling-at-start";

// Specifies a location for profiling output. This will only work if chrome
// has been built with the gyp variable profiling=1.
//   {pid} if present will be replaced by the pid of the process.
//   {count} if present will be incremented each time a profile is generated
//           for this process.
// The default is chrome-profile-{pid}.
const char kProfilingFile[]                 = "profiling-file";

// Controls whether profile data is periodically flushed to a file.
// Normally the data gets written on exit but cases exist where chrome
// doesn't exit cleanly (especially when using single-process).
// A time in seconds can be specified.
const char kProfilingFlush[]                = "profiling-flush";

// Force proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specify a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are
// also specified.
// This is a comma separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[]               = "proxy-bypass-list";

// Use the pac script at the given URL
const char kProxyPacUrl[]                   = "proxy-pac-url";

// Use a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[]                   = "proxy-server";

// Adds a "Purge memory" button to the Task Manager, which tries to dump as
// much memory as possible.  This is mostly useful for testing how well the
// MemoryPurger functionality works.
//
// NOTE: This is only implemented for Views.
const char kPurgeMemoryButton[]             = "purge-memory-button";

// Reload pages that have been killed when they are next focused by the user.
const char kReloadKilledTabs[]              = "reload-killed-tabs";

// Enable remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

// Enable remote debug / automation shell on the specified port.
const char kRemoteShellPort[]               = "remote-shell-port";

// Indicates the last session should be restored on startup. This overrides
// the preferences value and is primarily intended for testing. The value of
// this switch is the number of tabs to wait until loaded before
// 'load completed' is sent to the ui_test.
const char kRestoreLastSession[]            = "restore-last-session";

// Restrict the automatic loading of web pages when typing in the omnibox to
// only search pages, and disable it for all other types (URLs, history pages,
// extensions, etc). Only has an effect if Instant is turned on (via "Enable
// Instant for faster searching and browsing" in Preferences -> Basics).
const char kRestrictInstantToSearch[]       = "restrict-instant-to-search";

// URL prefix used by safebrowsing to fetch hash, download data and
// report malware.
const char kSbInfoURLPrefix[] = "safebrowsing-info-url-prefix";

// URL prefix used by safebrowsing to get MAC key.
const char kSbMacKeyURLPrefix[] = "safebrowsing-mackey-url-prefix";

// If present, safebrowsing only performs update when
// SafeBrowsingProtocolManager::ForceScheduleNextUpdate() is explicitly called.
// This is used for testing only.
const char kSbDisableAutoUpdate[] = "safebrowsing-disable-auto-update";

// TODO(lzheng): Remove this flag once the feature works fine
// (http://crbug.com/74848).
// This flag disables safebrowsing feature that checks download url and download
// content's hash to make sure the content are not malicious.
const char kSbDisableDownloadProtection[] =
    "safebrowsing-disable-download-protection";

// Enable support for SDCH filtering (dictionary based expansion of content).
// Optional argument is *the* only domain name that will have SDCH suppport.
// Default is  "-enable-sdch" to advertise SDCH on all domains.
// Sample usage with argument: "-enable-sdch=.google.com"
// SDCH is currently only supported server-side for searches on google.com.
const char kSdchFilter[]                    = "enable-sdch";

// Enables the showing of an info-bar instructing user they can search directly
// from the omnibox.
const char kSearchInOmniboxHint[]           = "search-in-omnibox-hint";

// The LSID of the account to use for the service process.
const char kServiceAccountLsid[]            = "service-account-lsid";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Renders a border around composited Render Layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[]    = "show-composited-layer-borders";

// Draws a textual dump of the compositor layer tree to help debug and study
// layer compositing.
const char kShowCompositedLayerTree[]       = "show-composited-layer-tree";

// Draws a FPS indicator
const char kShowFPSCounter[]                = "show-fps-counter";

// Change the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when --enable-dcheck is
// specified.
const char kSilentDumpOnDCHECK[]            = "silent-dump-on-dcheck";

// Replaces the buffered data source for <audio> and <video> with a simplified
// resource loader that downloads the entire resource into memory.

// Start the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Allow insecure XMPP connections for sync (for testing).
const char kSyncAllowInsecureXmppConnection[] =
    "sync-allow-insecure-xmpp-connection";

// Invalidate any login info passed into sync's XMPP connection.
const char kSyncInvalidateXmppLogin[]       = "sync-invalidate-xmpp-login";

// Use the SyncerThread implementation that matches up with the old pthread
// impl semantics, but using Chrome synchronization primitives.  The only
// difference between this and the default is that we now have no timeout on
// Stop().  Should only use if you experience problems with the default.
const char kSyncerThreadTimedStop[]         = "syncer-thread-timed-stop";

// Override the default notification method for sync.
const char kSyncNotificationMethod[]        = "sync-notification-method";

// Override the default host used for sync notifications.  Can be either
// "host" or "host:port".
const char kSyncNotificationHost[]          = "sync-notification-host";

// Override the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Try to connect to XMPP using SSLTCP first (for testing).
const char kSyncTrySsltcpFirstForXmpp[]     = "sync-try-ssltcp-first-for-xmpp";

// Pass the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Runs the security test for the NaCl loader sandbox.
const char kTestNaClSandbox[]               = "test-nacl-sandbox";

// Pass the type of the current test harness ("browser" or "ui")
const char kTestType[]                      = "test-type";

// The value of this switch tells the app to listen for and broadcast
// testing-related messages on IPC channel with the given ID.
const char kTestingChannelID[]              = "testing-channel";

// Excludes these plugins from the plugin sandbox.
// This is a comma-separated list of plugin library names.
const char kTrustedPlugins[]                = "trusted-plugins";

// Experimental. Shows a dialog asking the user to try chrome. This flag
// is to be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Use a pure Views implementation when available (rather rather than platform
// native implementation such as GTK).
const char kUsePureViews[]                  = "use-pure-views";

// Use Spdy for the transport protocol instead of HTTP.
// This is a temporary testing flag.
const char kUseSpdy[]                       = "use-spdy";

// Ignore certificate related errors.
const char kIgnoreCertificateErrors[]       = "ignore-certificate-errors";

// Set the maximum SPDY sessions per domain.
const char kMaxSpdySessionsPerDomain[]      = "max-spdy-sessions-per-domain";

// Set the maximum concurrent streams over a SPDY session.
const char kMaxSpdyConcurrentStreams[]      = "max-spdy-concurrent-streams";

// Specifies the user data directory, which is where the browser will look
// for all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// directory to locate user scripts in as an over-ride of the default
const char kUserScriptsDir[]                = "user-scripts-dir";

// On POSIX only: the contents of this flag are prepended to the utility
// process command line. Useful values might be "valgrind" or "xterm -e gdb
// --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Print version information and quit.
const char kVersion[]                       = "version";

// Use WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is
// to use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

#if defined(OS_CHROMEOS)
// Enable DOM based login screens.
const char kDOMLogin[]                      = "dom-login";

// Enables device policy support on ChromeOS.
const char kEnableDevicePolicy[]            = "enable-device-policy";

// Enables VPN support on ChromeOS.
const char kEnableVPN[]                     = "enable-vpn";

// Enable the redirection of viewable document requests to the Google
// Document Viewer.
const char kEnableGView[]                   = "enable-gview";

// Should we show the image based login?
const char kEnableLoginImages[]             = "enable-login-images";

// Enable Chrome-as-a-login-manager behavior.
const char kLoginManager[]                  = "login-manager";

// Allows to override the first login screen. The value should be the name
// of the first login screen to show (see
// chrome/browser/chromeos/login/login_wizard_view.cc for actual names).
// Ignored if kLoginManager is not specified.
// TODO(avayvod): Remove when the switch is no longer needed for testing.
const char kLoginScreen[]                   = "login-screen";

// Allows control over the initial login screen size. Pass width,height.
const char kLoginScreenSize[]               = "login-screen-size";

// Attempts to load libcros and validate it, then exits. A nonzero return code
// means the library could not be loaded correctly.
const char kTestLoadLibcros[]               = "test-load-libcros";

// Specifies the profile to use once a chromeos user is logged in.
const char kLoginProfile[]                  = "login-profile";

// Specifies the user which is already logged in.
const char kLoginUser[]                     = "login-user";
// Specifies a password to be used to login (along with login-user).
const char kLoginPassword[]                 = "login-password";

// Allows to emulate situation when user logins with new password.
const char kLoginUserWithNewPassword[]      = "login-user-with-new-password";

// Attempts to perform Chrome OS offline and online login in parallel.
const char kParallelAuth[]                  = "parallel-auth";

// Use the frame layout used in chromeos.
const char kChromeosFrame[]                 = "chromeos-frame";

// Use the given language for UI in the input method candidate window.
const char kCandidateWindowLang[]           = "lang";

// Indicates that the browser is in "browse without sign-in" (Guest session)
// mode. Should completely disable extensions, sync and bookmarks.
const char kGuestSession[]                  = "bwsi";

// Indicates that stub implementations of the libcros library should be used.
// This is typically used to test the chromeos build of chrome on the desktop.
const char kStubCros[]                      = "stub-cros";

// URL of the html page for Screen Saver.
const char kScreenSaverUrl[]                = "screen-saver-url";

// Flag to trigger ChromeOS system log compression during feedback submit.
const char kCompressSystemFeedback[]        = "compress-sys-feedback";

// Flag to skip loading ChromeOS specific component extensions. This one is
// needed to prevent these component interfering with the some of the tests.
// TODO(zelidrag): http://crosbug.com/14463 - we should remove this switch once
// get rid of ChromeOS component extensions with background pages.
const char kSkipChromeOSComponents[]        = "skip-chromeos-components";

// Forces usage of libcros stub implementation. For testing purposes, this
// switch separates chrome code from the rest of ChromeOS.
const char kForceStubLibcros[]              = "force-stub-libcros";

// Enables Advanced File System.
const char kEnableAdvancedFileSystem[]      = "enable-advanced-fs";
#endif

#if defined(OS_LINUX)
// Specify the amount the trackpad should scroll by.
const char kScrollPixels[]                  = "scroll-pixels";
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
// Use the system SSL library (Secure Transport on Mac, SChannel on Windows)
// instead of NSS for SSL.
const char kUseSystemSSL[]                  = "use-system-ssl";
#endif

#if defined(OS_POSIX)
// A flag, generated internally by Chrome for renderer and other helper process
// command lines on Linux and Mac.  It tells the helper process to enable crash
// dumping and reporting, because helpers cannot access the profile or other
// files needed to make this decision.
const char kEnableCrashReporter[]           = "enable-crash-reporter";

// Bypass the error dialog when the profile lock couldn't be attained.
// This switch is used during automated testing.
const char kNoProcessSingletonDialog[]      = "no-process-singleton-dialog";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// Specifies which password store to use (detect, default, gnome, kwallet).
const char kPasswordStore[]                 = "password-store";
#endif
#endif

#if defined(OS_MACOSX)
// Enables the tabs expose feature ( http://crbug.com/50307 ).
const char kEnableExposeForTabs[]           = "enable-expose-for-tabs";
#endif

#if !defined(OS_MACOSX)
// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";
#endif

// Enables debug paint in views framework. Enabling this causes the damaged
// region being painted to flash in red.
#if defined(TOOLKIT_VIEWS)
const char kDebugViewsPaint[]               = "debug-views-paint";
#endif

// Debug only switch to prevent the mouse cursor from disappearing when
// touch is enabled
#if defined(TOUCH_UI)
const char kKeepMouseCursor[]               = "keep-mouse-cursor";
#endif

#ifndef NDEBUG
// Clear the token service before using it.  This allows simulating
// the expiration of credentials during testing.
const char kClearTokenService[]             = "clear-token-service";

// Sets a token in the token service, for testing.
const char kSetToken[]                      = "set-token";

// Debug only switch to specify which websocket live experiment host to be used.
// If host is specified, it also makes initial delay shorter (5 min to 5 sec)
// to make it faster to test websocket live experiment code.
const char kWebSocketLiveExperimentHost[]   = "websocket-live-experiment-host";

// Debug only switch to give access to all private extension APIs to
// any non-component extension that is requesting it.
const char kExposePrivateExtensionApi[]   = "expose-private-extension-api";
#endif

#if defined(HAVE_XINPUT2)
// Tells chrome to interpret events from these devices as touch events. Only
// available with XInput 2 (i.e. X server 1.8 or above). The id's of the devices
// can be retrieved from 'xinput list'.
const char kTouchDevices[]                  = "touch-devices";
#endif

#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
// Disable print preview (Not exposed via about:flags. Only used for testing.)
const char kDisablePrintPreview[]           = "disable-print-preview";

bool IsPrintPreviewEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(kDisablePrintPreview);
}
#else
// Enable print preview (no PDF viewer, thus not supported with Chromium).
const char kEnablePrintPreview[]            = "enable-print-preview";

bool IsPrintPreviewEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(kEnablePrintPreview);
}
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR CRAP TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead,
// please put them in alphabetical order above, or in order inside the
// appropriate ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
