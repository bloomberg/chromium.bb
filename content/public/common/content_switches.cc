// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

namespace switches {

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Enumerates and prints a child process' most dangerous handles when it
// is terminated.
const char kAuditHandles[]                  = "enable-handle-auditing";

// The same as kAuditHandles except all handles are enumerated.
const char kAuditAllHandles[]               = "enable-handle-auditing-all";

// Causes the browser process to throw an assertion on startup.
const char kBrowserAssertTest[]             = "assert-test";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Run Chrome in Chrome Frame mode. This means that Chrome expects to be run
// as a dependent process of the Chrome Frame plugin.
const char kChromeFrame[]                   = "chrome-frame";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Disables accelerated compositing.
const char kDisableAcceleratedCompositing[] = "disable-accelerated-compositing";

// Disables the hardware acceleration of 3D CSS and animation.
const char kDisableAcceleratedLayers[]      = "disable-accelerated-layers";

// Disables the hardware acceleration of plugins.
const char kDisableAcceleratedPlugins[]     = "disable-accelerated-plugins";

// Disables GPU accelerated video display.
const char kDisableAcceleratedVideo[]       = "disable-accelerated-video";

// Disables the alternate window station for the renderer.
const char kDisableAltWinstation[]          = "disable-winsta";

// Disable the ApplicationCache.
const char kDisableApplicationCache[]       = "disable-application-cache";
//
// TODO(scherkus): remove --disable-audio when we have a proper fallback
// mechanism.
const char kDisableAudio[]                  = "disable-audio";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables data transfer items.
const char kDisableDataTransferItems[]      = "disable-data-transfer-items";

// Disables desktop notifications (default enabled on windows).
const char kDisableDesktopNotifications[]   = "disable-desktop-notifications";

// Disables device orientation events.
const char kDisableDeviceOrientation[]      = "disable-device-orientation";

// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Suppresses support for the Geolocation javascript API.
const char kDisableGeolocation[]            = "disable-geolocation";

// Disable GL multisampling.
const char kDisableGLMultisampling[]        = "disable-gl-multisampling";

// Disable the GLSL translator.
const char kDisableGLSLTranslator[]         = "disable-glsl-translator";

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[]            = "disable-gpu-watchdog";

// Disable the Indexed Database API.
const char kDisableIndexedDatabase[]        = "disable-indexed-database";

// Prevent Java from running.
const char kDisableJava[]                   = "disable-java";

// Don't execute JavaScript (browser JS like the new tab page still runs).
const char kDisableJavaScript[]             = "disable-javascript";

// Disable JavaScript I18N API.
const char kDisableJavaScriptI18NAPI[]      = "disable-javascript-i18n-api";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Prevent plugins from running.
const char kDisablePlugins[]                = "disable-plugins";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Disable False Start in SSL and TLS connections.
const char kDisableSSLFalseStart[]          = "disable-ssl-false-start";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disable the seccomp sandbox (Linux only)
const char kDisableSeccompSandbox[]         = "disable-seccomp-sandbox";

// Disable session storage.
const char kDisableSessionStorage[]         = "disable-session-storage";

// Enable shared workers. Functionality not yet complete.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disables speech input.
const char kDisableSpeechInput[]            = "disable-speech-input";

// Disable Spellcheck API.
const char kDisableSpellcheckAPI[]          = "disable-spellcheck-api";

// Disable web audio API.
const char kDisableWebAudio[]               = "disable-webaudio";

// Disable Web Sockets support.
const char kDisableWebSockets[]             = "disable-web-sockets";

// Enable gpu-accelerated 2d canvas.
const char kEnableAccelerated2dCanvas[]     = "enable-accelerated-2d-canvas";

// Enable hardware accelerated page drawing.
// Please note that this flag is honored only if chromium is compiled with
// SKIA_GPU flag, which can be enabled by setting use_skia_gpu variable to 1
// in build/features_override.gypi.
const char kEnableAcceleratedDrawing[]      = "enable-accelerated-drawing";

// Enables WebKit accessibility within the renderer process.
const char kEnableAccessibility[]           = "enable-accessibility";

// Turns on extremely verbose logging of accessibility events.
const char kEnableAccessibilityLogging[]    = "enable-accessibility-logging";

// Enables the creation of compositing layers for fixed position elements.
const char kEnableCompositingForFixedPosition[] =
     "enable-fixed-position-compositing";

// Enable DNS side checking of certificates. Still experimental, should only
// be used by developers at the current time.
const char kEnableDNSCertProvenanceChecking[] =
     "enable-dns-cert-provenance-checking";

// Enables device motion events.
const char kEnableDeviceMotion[]            = "enable-device-motion";

// Enable the JavaScript Full Screen API.
const char kDisableFullScreen[]             = "disable-fullscreen";

// Enable the JavaScript Pointer Lock API.
const char kEnablePointerLock[]             = "enable-pointer-lock";

// Enable the Gamepad API
const char kEnableGamepad[]                 = "enable-gamepad";

// Enable the GPU plugin and Pepper 3D rendering.
const char kEnableGPUPlugin[]               = "enable-gpu-plugin";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Enables Media Source API on <audio>/<video> elements.
const char kEnableMediaSource[]              = "enable-media-source";

// Enable media stream in WebKit.
// http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#mediastream
const char kEnableMediaStream[]             = "enable-media-stream";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Enables TLS origin bound certificate extension.
const char kEnableOriginBoundCerts[]  = "enable-origin-bound-certs";

// Enable caching of pre-parsed JS script data.  See http://crbug.com/32407.
const char kEnablePreparsedJsCaching[]      = "enable-preparsed-js-caching";

// Enable privileged WebGL extensions; without this switch such extensions are
// available only to Chrome extensions.
const char kEnablePrivilegedWebGLExtensions[] =
    "enable-privileged-webgl-extensions";

// Enables TLS cached info extension.
const char kEnableSSLCachedInfo[]  = "enable-ssl-cached-info";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enable the seccomp sandbox (Linux only)
const char kEnableSeccompSandbox[]          = "enable-seccomp-sandbox";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Experimentally ensure each renderer process has pages from only one site.
// This is expected to break compatibility with many pages for now.
const char kEnableStrictSiteIsolation[]     = "enable-strict-site-isolation";

// Enable multithreaded GPU compositing of web content.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Enable support for JavaScript touch events.
const char kEnableTouchEvents[]             = "enable-touch-events";

// Enables support for fullscreen video. Current implementation is
// incomplete and this flag is used for development and testing.
const char kEnableVideoFullscreen[]         = "enable-video-fullscreen";

// Enables video logging where video elements log playback performance data to
// the debug log.
const char kEnableVideoLogging[]            = "enable-video-logging";

// Enables support for video tracks. Current implementation is
// incomplete and this flag is used for development and testing.
const char kEnableVideoTrack[]              = "enable-video-track";

// Enable Web Intents.
const char kEnableWebIntents[]              = "enable-web-intents";

// Enables experimental features for the geolocation API.
// Current features:
// - CoreLocation support for Mac OS X 10.6
// - Gateway location for Linux and Windows
// - Location platform support for Windows 7
const char kExperimentalLocationFeatures[]  = "experimental-location-features";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// Some field tests may rendomized in the browser, and the randomly selected
// outcome needs to be propagated to the renderer.  For instance, this is used
// to modify histograms recorded in the renderer, or to get the renderer to
// also set of its state (initialize, or not initialize components) to match the
// experiment(s).
// The argument is a string-ized list of experiment names, and the associated
// value that was randomly selected.  In the recent implementetaion, the
// persistent representation generated by field_trial.cc and later decoded, is a
// list of name and value pairs, separated by slashes. See field trial.cc for
// current details.
const char kForceFieldTestNameAndValue[]    = "force-fieldtest";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[]            = "ignore-gpu-blacklist";

// Run the GPU process as a thread in the browser process.
const char kInProcessGPU[]                  = "in-process-gpu";

// Runs plugins inside the renderer process
const char kInProcessPlugins[]              = "in-process-plugins";

// Runs WebGL inside the renderer process.
const char kInProcessWebGL[]                = "in-process-webgl";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Make plugin processes log their sent and received messages to VLOG(1).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Uses high-latency IPC for audio.
// That is temporary until we are sure low latency works and remove old code.
const char kHighLatencyAudio[]              = "disable-low-latency-audio";

// Causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// Causes the process to run as a NativeClient loader.
const char kNaClLoaderProcess[]             = "nacl-loader";

// Support a separate switch that enables the v8 playback extension.
// The extension causes javascript calls to Date.now() and Math.random()
// to return consistent values, such that subsequent loads of the same
// page will result in consistent js-generated data and XHR requests.
// Pages may still be able to generate inconsistent data from plugins.
const char kNoJsRandomness[]                = "no-js-randomness";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Disables the sandbox for all process types that are normally sandboxed.
const char kNoSandbox[]                     = "no-sandbox";

// Read previously recorded data from the cache. Only cached data is read.
// See kRecordMode.
const char kPlaybackMode[]                  = "playback-mode";

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

// Argument to the process type that indicates a PPAPI broker process type.
const char kPpapiBrokerProcess[]            = "ppapi-broker";

// "Command-line" arguments for the PPAPI Flash; used for debugging options.
const char kPpapiFlashArgs[]                = "ppapi-flash-args";

// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[]                = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[]             = "ppapi-flash-version";

// Runs PPAPI (Pepper) plugins out-of-process.
const char kPpapiOutOfProcess[]             = "ppapi-out-of-process";

// Like kPluginLauncher for PPAPI plugins.
const char kPpapiPluginLauncher[]           = "ppapi-plugin-launcher";

// Argument to the process type that indicates a PPAPI plugin process type.
const char kPpapiPluginProcess[]            = "ppapi";

// Causes the PPAPI sub process to display a dialog on launch.
const char kPpapiStartupDialog[]            = "ppapi-startup-dialog";

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

// Chrome supports a playback and record mode.  Record mode saves *everything*
// to the cache.  Playback mode reads data exclusively from the cache.  This
// allows us to record a session into the cache and then replay it at will.
// See also kPlaybackMode.
const char kRecordMode[]                    = "record-mode";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Register Pepper plugins (see pepper_plugin_registry.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

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

// Causes the process to run as a service process.
const char kServiceProcess[]                = "service";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";
//
// TODO(scherkus): remove --simple-data-source when our media resource loading
// is cleaned up and playback testing completed.
const char kSimpleDataSource[]              = "simple-data-source";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Skip gpu info collection, blacklist loading, and blacklist auto-update
// scheduling at browser startup time.
// Therefore, all GPU features are available, and about:gpu page shows empty
// content. The switch is intended only for tests.
const char kSkipGpuDataLoading[]            = "skip-gpu-data-loading";

// Runs the security test for the renderer sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// Causes TRACE_EVENT flags to be recorded from startup. Optionally, can
// specify the specific trace categories to include (e.g.
// --trace-startup=base,net) otherwise, all events are recorded. Setting this
// flag results in the first call to BeginTracing() to receive all trace events
// since startup. In Chrome, you may find --trace-startup-file and
// --trace-startup-duration to control the auto-saving of the trace (not
// supported in the base-only TraceLog component).
const char kTraceStartup[]                  = "trace-startup";

// If supplied, sets the file which startup tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-startup is also supplied.
// Example: --trace-startup --trace-startup-file=/tmp/trace_event.log
// As a special case, can be set to 'none' - this disables automatically saving
// the result to a file and the first manually recorded trace will then receive
// all events since startup.
const char kTraceStartupFile[]              = "trace-startup-file";

// Sets the time in seconds until startup tracing ends. If omitted a default of
// 5 seconds is used. Has no effect without --trace-startup, or if
// --startup-trace-file=none was supplied.
const char kTraceStartupDuration[]          = "trace-startup-duration";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

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

// Choose which logging channels in WebCore to activate.  See
// Logging.cpp in WebKit's WebCore for a list of available channels.
const char kWebCoreLogChannels[]            = "webcore-log-channels";

// Causes the worker process allocation to use as many processes as cores.
const char kWebWorkerProcessPerCore[]       = "web-worker-process-per-core";

// Causes workers to run together in one process, depending on their domains.
// Note this is duplicated in webworkerclient_impl.cc
const char kWebWorkerShareProcesses[]       = "web-worker-share-processes";

// Causes the process to run as a worker subprocess.
const char kWorkerProcess[]                 = "worker";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

// Enables moving cursor by word in visual order.
const char kEnableVisualWordMovement[]      = "enable-visual-word-movement";

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// Specify the amount the trackpad should scroll by.
const char kScrollPixels[]                  = "scroll-pixels";
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
// Use the system SSL library (Secure Transport on Mac, SChannel on Windows)
// instead of NSS for SSL.
const char kUseSystemSSL[]                  = "use-system-ssl";
#endif

#if !defined(OFFICIAL_BUILD)
// Causes the renderer process to throw an assertion on launch.
const char kRendererCheckFalseTest[]        = "renderer-check-false-test";
#endif

}  // namespace switches
