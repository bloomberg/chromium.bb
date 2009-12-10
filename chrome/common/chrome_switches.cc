// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.
// -----------------------------------------------------------------------------

// Activate (make foreground) myself on launch.  Helpful when Chrome
// is launched on the command line (e.g. by Selenium).  Only needed on Mac.
const char kActivateOnLaunch[] = "activate-on-launch";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Enable web inspector for all windows, even if they're part of the browser.
// Allows us to use our dev tools to debug browser windows itself.
const char kAlwaysEnableDevTools[]          = "always-enable-dev-tools";

// Specifies that the associated value should be launched in "application" mode.
const char kApp[]                           = "app";

// The value of this switch tells the app to listen for and broadcast
// automation-related messages on IPC channel with the given ID.
const char kAutomationClientChannelID[]     = "automation-channel";

// Enables the bookmark menu.
const char kBookmarkMenu[]                  = "bookmark-menu";

// Causes the browser process to throw an assertion on startup.
const char kBrowserAssertTest[]             = "assert-test";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Run Chrome in Chrome Frame mode. This means that Chrome expects to be run
// as a dependent process of the Chrome Frame plugin.
const char kChromeFrame[]                   = "chrome-frame";

// The Country we should use.  This is normally obtained from the operating
// system during first run and cached in the preferences afterwards.  This is a
// string value, the 2 letter code from ISO 3166-1.
const char kCountry[]                       = "country";

// Enables support to debug printing subsystem.
const char kDebugPrint[]                    = "debug-print";

// Triggers a pletora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Disables the alternate window station for the renderer.
const char kDisableAltWinstation[]          = "disable-winsta";

// Replaces the audio IPC layer for <audio> and <video> with a mock audio
// device, useful when using remote desktop or machines without sound cards.
// This is temporary until we fix the underlying problem.
//
// TODO(scherkus): remove --disable-audio when we have a proper fallback
// mechanism.
const char kDisableAudio[]                  = "disable-audio";

// Disable support for cached byte-ranges.
const char kDisableByteRangeSupport[]       = "disable-byte-range-support";

// Disables the custom JumpList on Windows 7.
const char kDisableCustomJumpList[]         = "disable-custom-jumplist";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables desktop notifications (default enabled on windows).
const char kDisableDesktopNotifications[]   = "disable-desktop-notifications";

// Browser flag to disable the web inspector for all renderers.
const char kDisableDevTools[]               = "disable-dev-tools";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Suppresses hang monitor dialogs in renderer processes.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Don't resolve hostnames to IPv6 addresses. This can be used when debugging
// issues relating to IPv6, but shouldn't otherwise be needed. Be sure to
// file bugs if something isn't working properly in the presence of IPv6.
const char kDisableIPv6[]                   = "disable-ipv6";

// Prevent images from loading.
const char kDisableImages[]                 = "disable-images";

// Don't execute JavaScript (browser JS like the new tab page still runs).
const char kDisableJavaScript[]             = "disable-javascript";

// Prevent Java from running.
const char kDisableJava[]                   = "disable-java";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Completely disables UMA metrics system.
const char kDisableMetrics[]                = "disable-metrics";

// Whether we should prevent the new tab page from showing the first run
// notification.
const char kDisableNewTabFirstRun[]         = "disable-new-tab-first-run";

// Prevent plugins from running.
const char kDisablePlugins[]                = "disable-plugins";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disable remote web font support. SVG font should always work whether
// this option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Enable shared workers. Functionality not yet complete.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disable site-specific tailoring to compatibility issues in WebKit.
const char kDisableSiteSpecificQuirks[]     = "disable-site-specific-quirks";

// Disable syncing bookmarks to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Enables the backend service for web resources, used in the new tab page for
// loading tips and recommendations from a JSON feed.
const char kDisableWebResources[]           = "disable-web-resources";

// Don't enforce the same-origin policy.  (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disable Web Sockets support.
const char kDisableWebSockets[]             = "disable-web-sockets";

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

// Specifies if the dom_automation_controller_ needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating
// dom based tests.
const char kDomAutomationController[]       = "dom-automation";

// Dump any accumualted histograms to the log when browser terminates (requires
// logging to be enabled to really do anything).  Used by developers and test
// scripts.
const char kDumpHistogramsOnExit[]          = "dump-histograms-on-exit";

// Enable ApplicationCache. Still mostly not there.
const char kEnableApplicationCache[]        = "enable-application-cache";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enable experimental WebGL support.
const char kEnableExperimentalWebGL[]       = "enable-webgl";

// Enable experimental timeline API.
const char kEnableExtensionTimelineApi[]    = "enable-extension-timeline-api";

// Enable extension toolstrips (deprecated API - will be removed).
const char kEnableExtensionToolstrips[]     = "enable-extension-toolstrips";

// Enable the fastback page cache.
const char kEnableFastback[]                = "enable-fastback";

// By default, cookies are not allowed on file://. They are needed for
// testing, for example page cycler and layout tests.  See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enable Geolocation support.
const char kEnableGeolocation[]       = "enable-geolocation";

// Disable LocalStorage.
const char kDisableLocalStorage[]            = "disable-local-storage";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Enable Native Web Worker support.
const char kEnableNativeWebWorkers[]        = "enable-native-web-workers";

// Enable AutoFill++.
const char kEnableNewAutoFill[]             = "enable-new-autofill";

// Enable Privacy Blacklists.
const char kEnablePrivacyBlacklists[]       = "enable-privacy-blacklists";

// Turns on the accessibility in the renderer.  Off by default until
// http://b/issue?id=1432077 is fixed.
const char kEnableRendererAccessibility[]   = "enable-renderer-accessibility";

// Enable the seccomp sandbox (Linux only)
const char kEnableSeccompSandbox[]          = "enable-seccomp-sandbox";

// Enable session storage.  Still buggy.
const char kEnableSessionStorage[]          = "enable-session-storage";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Enable syncing bookmarks to a Google Account.
const char kEnableSync[]                    = "enable-sync";

// Whether the multiple profiles feature based on the user-data-dir flag is
// enabled or not.
const char kEnableUserDataDirProfiles[]     = "enable-udd-profiles";

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

// Causes the process to run as an extension subprocess.
const char kExtensionProcess[]              = "extension";

// Frequency in seconds for Extensions auto-update.
const char kExtensionsUpdateFrequency[]     = "extensions-update-frequency";

// The file descriptor limit is set to the value of this switch, subject to the
// OS hard limits. Useful for testing that file descriptor exhaustion is handled
// gracefully.
const char kFileDescriptorLimit[]           = "file-descriptor-limit";

// Display the First Run experience when the browser is started, regardless of
// whether or not it's actually the first run.
const char kFirstRun[]                      = "first-run";

// Some field tests may rendomized in the browser, and the randomly selected
// outcome needs to be propogated to the renderer.  For instance, this is used
// to modify histograms recorded in the renderer, or to get the renderer to
// also set of its state (initialize, or not initialize components) to match the
// experiment(s).
// The argument is a string-ized list of experiment names, and the associated
// value that was randomly selected.  In the recent implementetaion, the
// persistent representation generated by field_trial.cc and later decoded, is a
// list of name and value pairs, separated by slashes. See field trial.cc for
// current details.
const char kForceFieldTestNameAndValue[]    = "force-fieldtest";

// Make Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This
// only shows an error box because the only way to hide Chrome is by
// uninstalling it.
const char kHideIcons[]                     = "hide-icons";

// The value of this switch specifies which page will be displayed
// in newly-opened tabs.  We need this for testing purposes so
// that the UI tests don't depend on what comes up for http://google.com.
const char kHomePage[]                      = "homepage";

// Perform importing from another browser. The value associated with this
// setting encodes the target browser and what items to import.
const char kImport[]                        = "import";

// Runs plugins inside the renderer process
const char kInProcessPlugins[]              = "in-process-plugins";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Runs the Native Client inside the renderer process.
const char kInternalNaCl[]                  = "internal-nacl";

// Runs a trusted Pepper plugin inside the renderer process.
const char kInternalPepper[]                = "internal-pepper";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Load an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Will filter log messages to show only the messages that are prefixed
// with the specified value. See also kEnableLogging and kLoggingLevel.
const char kLogFilterPrefix[]               = "log-filter-prefix";

// Make plugin processes log their sent and received messages to LOG(INFO).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

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

// Causes the process to run as a NativeClient's sel_ldr subprocess.
const char kNaClProcess[]                   = "nacl";

// Allows the new tab page resource to be loaded from a local HTML file. This
// should be a path to the HTML file that you want to use for the new tab page.
// It is used for manually testing new versions of the new tab page only,
// performance will be poor.
const char kNewTabPage[]                    = "new-tab-page";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// Don't record/playback events when using record & playback.
const char kNoEvents[]                      = "no-events";

// Bypass the First Run experience when the browser is started, regardless of
// whether or not it's actually the first run. Overrides kFirstRun in case
// you're for some reason tempted to pass them both.
const char kNoFirstRun[]                    = "no-first-run";

// Support a separate switch that enables the v8 playback extension.
// The extension causes javascript calls to Date.now() and Math.random()
// to return consistent values, such that subsequent loads of the same
// page will result in consistent js-generated data and XHR requests.
// Pages may still be able to generate inconsistent data from plugins.
const char kNoJsRandomness[]                = "no-js-randomness";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Runs the renderer outside the sandbox.
const char kNoSandbox[]                     = "no-sandbox";

// Number of entries to show in the omnibox popup.
const char kOmniBoxPopupCount[]             = "omnibox-popup-count";

// Package an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key is to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Number of tabs to pin on startup. This is not use if session restore is
// enabled.
const char kPinnedTabCount[]                = "pinned-tab-count";

// Read previously recorded data from the cache. Only cached data is read.
// See kRecordMode.
const char kPlaybackMode[]                  = "playback-mode";

// Specifies the plugin data directory, which is where plugins (Gears
// specifically) will store its state.
const char kPluginDataDir[]                 = "plugin-data-dir";

// Specifies a command that should be used to launch the plugin process.  Useful
// for running the plugin process through purify or quantify.  Ex:
//   --plugin-launcher="path\to\purify /Run=yes"
const char kPluginLauncher[]                = "plugin-launcher";

// Tells the plugin process the path of the plugin to load
const char kPluginPath[]                    = "plugin-path";

// Causes the process to run as a plugin subprocess.
const char kPluginProcess[]                 = "plugin";

// Causes the plugin process to display a dialog on launch.
const char kPluginStartupDialog[]           = "plugin-startup-dialog";

// Prints the pages on the screen.
const char kPrint[] = "print";

// Runs a single process for each site (i.e., group of pages from the same
// registered domain) the user visits.  We default to using a renderer process
// for each site instance (i.e., group of pages from the same registered
// domain with script connections to each other).
const char kProcessPerSite[]                = "process-per-site";

// Runs each set of script-connected tabs (i.e., a BrowsingInstance) in its own
// renderer process.  We default to using a renderer process for each
// site instance (i.e., group of pages from the same registered domain with
// script connections to each other).
const char kProcessPerTab[]                 = "process-per-tab";

#if defined(OS_CHROMEOS)
// Overrides the Default profile.
const char kProfile[]                       = "profile";
#endif

// Causes the process to run as a profile import subprocess.
const char kProfileImportProcess[]          = "profile-import";

// Force proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specify a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are
// also specified.
// TODO(robertshield): Specify host format.
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

// Chrome supports a playback and record mode.  Record mode saves *everything*
// to the cache.  Playback mode reads data exclusively from the cache.  This
// allows us to record a session into the cache and then replay it at will.
// See also kPlaybackMode.
const char kRecordMode[]                    = "record-mode";

// Enable remote debug / automation shell on the specified port.
const char kRemoteShellPort[]               = "remote-shell-port";

// Causes the renderer process to throw an assertion on launch.
const char kRendererAssertTest[]            = "renderer-assert-test";

// On POSIX only: the contents of this flag are prepended to the renderer
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the renderer process to crash on launch.
const char kRendererCrashTest[]             = "renderer-crash-test";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Indicates the last session should be restored on startup. This overrides
// the preferences value and is primarily intended for testing. The value of
// this switch is the number of tabs to wait until loaded before
// 'load completed' is sent to the ui_test.
const char kRestoreLastSession[]            = "restore-last-session";

// Runs the plugin processes inside the sandbox.
const char kSafePlugins[]                   = "safe-plugins";

// Enable support for SDCH filtering (dictionary based expansion of content).
// Optional argument is *the* only domain name that will have SDCH suppport.
// Default is  "-enable-sdch" to advertise SDCH on all domains.
// Sample usage with argument: "-enable-sdch=.google.com"
// SDCH is currently only supported server-side for searches on google.com.
const char kSdchFilter[]                    = "enable-sdch";

// Enables the showing of an info-bar instructing user they can search directly
// from the omnibox.
const char kSearchInOmniboxHint[]           = "search-in-omnibox-hint";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Show extensions on top with toolbar.
const char kShowExtensionsOnTop[]           = "show-extensions-on-top";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Change the DCHECKS to dump memory and continue instead of displaying error
// dialog. This is valid only in Release mode when --enable-dcheck is
// specified.
const char kSilentDumpOnDCHECK[]            = "silent-dump-on-dcheck";

// Replaces the buffered data source for <audio> and <video> with a simplified
// resource loader that downloads the entire resource into memory.
//
// TODO(scherkus): remove --simple-data-source when our media resource loading
// is cleaned up and playback testing completed.
const char kSimpleDataSource[]              = "simple-data-source";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Start the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Override the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Use the SyncerThread implementation that matches up with the old pthread
// impl semantics, but using Chrome synchronization primitives.  The only
// difference between this and the default is that we now have no timeout on
// Stop().  Should only use if you experience problems with the default.
const char kSyncerThreadTimedStop[]         = "syncer-thread-timed-stop";

// Used to set the value of SessionRestore::num_tabs_to_load_. See
// session_restore.h for details.
const char kTabCountToLoadOnSessionRestore[]=
    "tab-count-to-load-on-session-restore";

// Pass the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Runs the security test for the sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// The value of this switch tells the app to listen for and broadcast
// testing-related messages on IPC channel with the given ID.
const char kTestingChannelID[]              = "testing-channel";

// Enables using ThumbnailStore instead of ThumbnailDatabase for setting and
// getting thumbnails for the new tab page.
const char kThumbnailStore[]                = "thumbnail-store";

// Excludes these plugins from the plugin sandbox.
// This is a comma-separated list of plugin library names.
const char kTrustedPlugins[]                = "trusted-plugins";

// Experimental. Shows a dialog asking the user to try chrome. This flag
// is to be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Use Flip for the transport protocol instead of HTTP.
// This is a temporary testing flag.
const char kUseFlip[]                       = "use-flip";

// Force all requests to go to this server.  This commandline is provided
// for testing purposes only, and will likely be removed soon.  It can also
// hurt startup performance as it does a synchronous name resolution on the
// UI thread.  The port is not optional.
// The host resolution using this scheme is done exactly once at startup.
// From that point on, it is completely a static configuration.
// TODO(mbelshe): Remove this flag when testing is complete.
//   --testing-fixed-server=myserver:1000
const char kFixedServer[]                   = "testing-fixed-server";

// Use the low fragmentation heap for the CRT.
const char kUseLowFragHeapCrt[]             = "use-lf-heap";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// Specifies the user data directory, which is where the browser will look
// for all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// directory to locate user scripts in as an over-ride of the default
const char kUserScriptsDir[]                = "user-scripts-dir";

// On POSIX only: the contents of this flag are prepended to the utility
// process command line. Useful values might be "valgrind" or "xterm -e gdb
// --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// Causes the worker process allocation to use as many processes as cores.
const char kWebWorkerProcessPerCore[]       = "web-worker-process-per-core";

// Causes workers to run together in one process, depending on their domains.
// Note this is duplicated in webworkerclient_impl.cc
const char kWebWorkerShareProcesses[]       = "web-worker-share-processes";

// Use WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is
// to use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

// On Windows only: use the old WinInet-based ftp implemetation.
const char kWininetFtp[]                    = "wininet-ftp";

// Causes the process to run as a worker subprocess.
const char kWorkerProcess[]                 = "worker";

// Causes the worker process to display a dialog on launch
const char kWorkerStartupDialog[]           = "worker-startup-dialog";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

#if defined(OS_CHROMEOS)
// The name of the pipe over which the Chrome OS login manager will send
// single-sign-on cookies.
const char kCookiePipe[]                    = "cookie-pipe";

// Enable the redirection of viewable document requests to the Google
// Document Viewer.
const char kEnableGView[]                   = "enable-gview";

// Attempts to load libcros and validate it, then exits. A nonzero return code
// means the library could not be loaded correctly.
const char kTestLoadLibcros[]               = "test-load-libcros";
#endif

#if defined(OS_LINUX)
// A temporary switch before we implement the client certificate selection UI.
// When an SSL server requests client authentication, select a client
// certificate automatically.
// WARNING: This switch has privacy issues because it reveals the user's
// identity to any server that requests a client certificate without the
// user's consent.
const char kAutoSSLClientAuth[]             = "auto-ssl-client-auth";
#endif

#if defined(OS_POSIX)
// Bypass the error dialog when the profile lock couldn't be attained.
// A flag, generated internally by Chrome for renderer and other helper process
// command lines on Linux and Mac.  It tells the helper process to enable crash
// dumping and reporting, because helpers cannot access the profile or other
// files needed to make this decision.
// If passed to the browser, it'll be passed on to all the helper processes
// as well, thereby force-enabling the crash reporter.
const char kEnableCrashReporter[]           = "enable-crash-reporter";

// This switch is used during automated testing.
const char kNoProcessSingletonDialog[]      = "no-process-singleton-dialog";
#endif

#if defined(OS_MACOSX)
// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";
#else
// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";
#endif

#ifndef NDEBUG
// Debug only switch to specify which gears plugin dll to load.
const char kGearsPluginPathOverride[]       = "gears-plugin-path";
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR CRAP TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead,
// please put them in alphabetical order above, or in order inside the
// appropriate ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
