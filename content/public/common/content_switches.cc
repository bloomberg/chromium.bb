// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

namespace switches {

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Allow compositing on chrome:// pages.
const char kAllowWebUICompositing[]         = "allow-webui-compositing";

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

// Cause a GPU hang to cause a crash dump, allowing for easier debugging of
// them.
const char kCrashOnGpuHang[]                = "crash-on-gpu-hang";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Disable antialiasing on 2d canvas.
const char kDisable2dCanvasAntialiasing[]   = "disable-canvas-aa";

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

// Disable deferred 2d canvas rendering.
const char kDisableDeferred2dCanvas[]        = "disable-deferred-2d-canvas";

// Disables desktop notifications (default enabled on windows).
const char kDisableDesktopNotifications[]   = "disable-desktop-notifications";

// Disables device orientation events.
const char kDisableDeviceOrientation[]      = "disable-device-orientation";

#if defined(OS_ANDROID)
// WebGL is disabled by default on Android.
const char kEnableExperimentalWebGL[]       = "enable-webgl";
#else
// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";
#endif

// Blacklist the GPU for accelerated compositing.
const char kBlacklistAcceleratedCompositing[] =
    "blacklist-accelerated-compositing";

// Blacklist the GPU for WebGL.
const char kBlacklistWebGL[]                = "blacklist-webgl";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable using 3D to present fullscreen flash
const char kDisableFlashFullscreen3d[]      = "disable-flash-fullscreen-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

// Suppresses support for the Geolocation javascript API.
const char kDisableGeolocation[]            = "disable-geolocation";

// Disable GL multisampling.
const char kDisableGLMultisampling[]        = "disable-gl-multisampling";

// Do not launch the GPU process shortly after browser process launch. Instead
// launch it when it is first needed.
const char kDisableGpuProcessPrelaunch[]    = "disable-gpu-process-prelaunch";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Reduces the GPU process sandbox to be less strict.
const char kReduceGpuSandbox[]              = "reduce-gpu-sandbox";

// Enable the GPU process sandbox (Linux/Chrome OS only for now).
const char kEnableGpuSandbox[]              = "enable-gpu-sandbox";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable the RenderThread's HistogramCustomizer.
const char kDisableHistogramCustomizer[]    = "disable-histogram-customizer";

// Disable the use of an ImageTransportSurface. This means the GPU process
// will present the rendered page rather than the browser process.
const char kDisableImageTransportSurface[]  = "disable-image-transport-surface";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[]                 = "use-gpu-in-tests";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[]            = "disable-gpu-watchdog";

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

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Disable False Start in SSL and TLS connections.
const char kDisableSSLFalseStart[]          = "disable-ssl-false-start";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disable the seccomp sandbox (Linux only)
const char kDisableSeccompSandbox[]         = "disable-seccomp-sandbox";

// Disable the seccomp filter sandbox (Linux only)
const char kDisableSeccompFilterSandbox[]   = "disable-seccomp-filter-sandbox";

// Disable session storage.
const char kDisableSessionStorage[]         = "disable-session-storage";

// Enable shared workers. Functionality not yet complete.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disables site-specific tailoring to compatibility issues in WebKit.
const char kDisableSiteSpecificQuirks[]     = "disable-site-specific-quirks";

// Disables speech input.
const char kDisableSpeechInput[]            = "disable-speech-input";

// Specifies the request key for the continuous speech recognition webservice.
const char kSpeechRecognitionWebserviceKey[] = "speech-service-key";

#if defined(OS_ANDROID)
// Enable web audio API.
const char kEnableWebAudio[]                = "enable-webaudio";

// WebRTC is disabled by default on Android.
const char kEnableWebRTC[]                  = "enable-webrtc";
#else
// Disable web audio API.
const char kDisableWebAudio[]               = "disable-webaudio";
#endif

// Don't enforce the same-origin policy. (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disable Web Sockets support.
const char kDisableWebSockets[]             = "disable-web-sockets";

// Disables WebKit's XSSAuditor. The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests. Also enables sending/receiving renderer automation messages
// through the |AutomationRenderViewHelper|.
//
// TODO(kkania): Rename this to enable-renderer-automation after moving the
// |DOMAutomationController| to the |AutomationRenderViewHelper|.
const char kDomAutomationController[]       = "dom-automation";

// Loosen security. Needed for tests using some of the functionality of
// |DOMAutomationController|.
const char kReduceSecurityForDomAutomationTests[] =
    "reduce-security-for-dom-automation-tests";

// Enable hardware accelerated page painting.
const char kEnableAcceleratedPainting[]     = "enable-accelerated-painting";

// Enable gpu-accelerated SVG/W3C filters.
const char kEnableAcceleratedFilters[]      = "enable-accelerated-filters";

// Turns on extremely verbose logging of accessibility events.
const char kEnableAccessibilityLogging[]    = "enable-accessibility-logging";

// Enables browser plugin compositing experiment.
const char kEnableBrowserPluginCompositing[] =
    "enable-browser-plugin-compositing";

// Enables browser plugin for all types of pages.
const char kEnableBrowserPluginForAllViewTypes[] =
    "enable-browser-plugin-for-all-view-types";

const char kEnableBrowserPluginPointerLock[] =
    "enable-browser-plugin-pointer-lock";

// Enable/Disable the creation of compositing layers for fixed position
// elements. Three options are needed to support four possible scenarios:
//  1. Default (disabled)
//  2. Enabled always (to allow dogfooding)
//  3. Disabled always (to give safety fallback for users)
//  4. Enabled only if we detect a highDPI display
//
// Option #4 may soon be the default, because the feature is needed soon for
// high DPI, but cannot be used (yet) for low DPI. Options #2 and #3 will
// override Option #4.
const char kEnableCompositingForFixedPosition[] =
     "enable-fixed-position-compositing";
const char kDisableCompositingForFixedPosition[] =
     "disable-fixed-position-compositing";
const char kEnableHighDpiCompositingForFixedPosition[] =
     "enable-high-dpi-fixed-position-compositing";

// Enables CSS3 custom filters
const char kEnableCssShaders[]              = "enable-css-shaders";

// Enables delegated renderer.
const char kEnableDelegatedRenderer[]       = "enable-delegated-renderer";

// Enables device motion events.
const char kEnableDeviceMotion[]            = "enable-device-motion";

// Enables restarting interrupted downloads.
const char kEnableDownloadResumption[]      = "enable-download-resumption";

// Enables WebKit features that are in development.
const char kEnableExperimentalWebKitFeatures[] =
    "enable-experimental-webkit-features";

// Disables the threaded HTML parser in WebKit
const char kDisableThreadedHTMLParser[]     = "disable-threaded-html-parser";

// Enables the fastback page cache.
const char kEnableFastback[]                = "enable-fastback";

// By default, a page is laid out to fill the entire width of the window.
// This flag fixes the layout of the page to a default of 980 CSS pixels,
// or to a specified width and height using --enable-fixed-layout=w,h
const char kEnableFixedLayout[]             = "enable-fixed-layout";

// Disable the JavaScript Full Screen API.
const char kDisableFullScreen[]             = "disable-fullscreen";

// Enable Text Service Framework(TSF) for text inputting instead of IMM32. This
// flag is ignored on Metro environment.
const char kEnableTextServicesFramework[] = "enable-text-services-framework";

// Enable Gesture Tap Highlight
const char kEnableGestureTapHighlight[]    = "enable-gesture-tap-highlight";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[]         = "enable-gpu-benchmarking";

// Enables TRACE for GL calls in the renderer.
const char kEnableGpuClientTracing[]        = "enable-gpu-client-tracing";

// Enables the memory benchmarking extension
const char kEnableMemoryBenchmarking[]      = "enable-memory-benchmarking";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Disable Media Source API on <audio>/<video> elements.
const char kDisableMediaSource[]             = "disable-media-source";

// Use fake device for MediaStream to replace actual camera and microphone.
const char kUseFakeDeviceForMediaStream[] = "use-fake-device-for-media-stream";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Enable caching of pre-parsed JS script data.  See http://crbug.com/32407.
const char kEnablePreparsedJsCaching[]      = "enable-preparsed-js-caching";

// Enable privileged WebGL extensions; without this switch such extensions are
// available only to Chrome extensions.
const char kEnablePrivilegedWebGLExtensions[] =
    "enable-privileged-webgl-extensions";

// Aggressively free GPU command buffers belonging to hidden tabs.
const char kEnablePruneGpuCommandBuffers[] =
    "enable-prune-gpu-command-buffers";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enables TLS cached info extension.
const char kEnableSSLCachedInfo[]  = "enable-ssl-cached-info";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enable the seccomp sandbox (Linux only)
const char kEnableSeccompSandbox[]          = "enable-seccomp-sandbox";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Experimentally ensures that each renderer process:
// 1) Only handles rendering for pages from a single site, apart from iframes.
// (Note that a page can reference content from multiple origins due to images,
// JavaScript files, etc.  Cross-site iframes are also loaded in-process.)
// 2) Only has authority to see or use cookies for the page's top-level origin.
// (So if a.com iframes b.com, the b.com network request will be sent without
// cookies.)
// This is expected to break compatibility with many pages for now.  Unlike the
// --site-per-process flag, this allows cross-site iframes, but it blocks all
// cookies on cross-site requests.
const char kEnableStrictSiteIsolation[]     = "enable-strict-site-isolation";

// Enable multithreaded GPU compositing of web content.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Allow GL contexts to be automatically virtualized (shared between command
// buffer clients) if they are compatible.
const char kEnableVirtualGLContexts[]       = "enable-virtual-gl-contexts";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]     = "disable-threaded-compositing";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Disables hardware acceleration of video decode, where available.
const char kDisableAcceleratedVideoDecode[] =
    "disable-accelerated-video-decode";

// Enables the use of the viewport meta tag, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enables experimental features for the geolocation API.
// Current features:
// - CoreLocation support for Mac OS X 10.6
// - Gateway location for Linux and Windows
// - Location platform support for Windows 7
const char kExperimentalLocationFeatures[]  = "experimental-location-features";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// If accelerated compositing is supported, always enter compositing mode for
// the base layer even when compositing is not strictly required.
const char kForceCompositingMode[]          = "force-compositing-mode";

// This flag disables force compositing mode and prevents it from being enabled
// via field trials.
const char kDisableForceCompositingMode[]   = "disable-force-compositing-mode";

// Some field trials may be randomized in the browser, and the randomly selected
// outcome needs to be propagated to the renderer. For instance, this is used
// to modify histograms recorded in the renderer, or to get the renderer to
// also set of its state (initialize, or not initialize components) to match the
// experiment(s). The option is also useful for forcing field trials when
// testing changes locally. The argument is a list of name and value pairs,
// separated by slashes. See FieldTrialList::CreateTrialsFromString() in
// field_trial.h for details.
const char kForceFieldTrials[]              = "force-fieldtrials";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// Force the compositor to use its software implementation instead of GL.
const char kEnableSoftwareCompositingGLAdapter[] =
    "enable-software-compositing-gl-adapter";

// Passes gpu device_id from browser process to GPU process.
const char kGpuDeviceID[]                   = "gpu-device-id";

// Passes gpu driver_vendor from browser process to GPU process.
const char kGpuDriverVendor[]               = "gpu-driver-vendor";

// Passes gpu driver_version from browser process to GPU process.
const char kGpuDriverVersion[]              = "gpu-driver-version";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Passes gpu vendor_id from browser process to GPU process.
const char kGpuVendorID[]                   = "gpu-vendor-id";

// These mappings only apply to the host resolver.
const char kHostResolverRules[]             = "host-resolver-rules";

// Ignores certificate-related errors.
const char kIgnoreCertificateErrors[]       = "ignore-certificate-errors";

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

// Sample memory usage with high frequency and store the results to the
// Renderer.Memory histogram. Used in memory tests.
const char kMemoryMetrics[]                 = "memory-metrics";

// Causes the process to run as a NativeClient broker
// (used for launching NaCl loader processes on 64-bit Windows).
const char kNaClBrokerProcess[]             = "nacl-broker";

// Causes the process to run as a NativeClient loader.
const char kNaClLoaderProcess[]             = "nacl-loader";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Disables the sandbox for all process types that are normally sandboxed.
const char kNoSandbox[]                     = "no-sandbox";

// Enables the sandboxed processes to run without a job object assigned to them.
// This flag is required to allow Chrome to run in RemoteApps or Citrix. This
// flag can reduce the security of the sandboxed processes and allow them to do
// certain API calls like shut down Windows or access the clipboard. Also we
// lose the chance to kill some processes until the outer job that owns them
// finishes.
const char kAllowNoSandboxJob[]             = "allow-no-sandbox-job";

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

// Runs PPAPI (Pepper) plugins in-process.
const char kPpapiInProcess[]                = "ppapi-in-process";

// Like kPluginLauncher for PPAPI plugins.
const char kPpapiPluginLauncher[]           = "ppapi-plugin-launcher";

// Argument to the process type that indicates a PPAPI plugin process type.
const char kPpapiPluginProcess[]            = "ppapi";

// Causes the PPAPI sub process to display a dialog on launch. Be sure to use
// --no-sandbox as well or the sandbox won't allow the dialog to display.
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

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Register Pepper plugins (see pepper_plugin_registry.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

// Causes the renderer process to throw an assertion on launch.
const char kRendererAssertTest[]            = "renderer-assert-test";

// On POSIX only: the contents of this flag are prepended to the renderer
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Causes the process to run as a service process.
const char kServiceProcess[]                = "service";

// Renders a border around composited Render Layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[]    = "show-composited-layer-borders";

// Draws a textual dump of the compositor layer tree to help debug and study
// layer compositing.
const char kShowCompositedLayerTree[]       = "show-composited-layer-tree";

// Draws a FPS indicator
const char kShowFPSCounter[]                = "show-fps-counter";

// Enables accelerated compositing for overflow scroll. Promotes eligible
// overflow:scroll elements to layers to enable accelerated scrolling for them.
const char kEnableAcceleratedOverflowScroll[] =
    "enable-accelerated-overflow-scroll";

// Disables accelerated compositing for overflow scroll.
extern const char kDisableAcceleratedOverflowScroll[] =
    "disable-accelerated-overflow-scroll";

// Enables accelerated compositing for scrollable frames for accelerated
// scrolling for them. Requires kForceCompositingMode.
const char kEnableAcceleratedScrollableFrames[] =
     "enable-accelerated-scrollable-frames";

// Enables accelerated scrolling by the compositor for frames. Requires
// kForceCompositingMode and kEnableAcceleratedScrollableFrames.
const char kEnableCompositedScrollingForFrames[] =
     "enable-composited-scrolling-for-frames";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Map mouse input events into touch gesture events.  Useful for debugging touch
// gestures without needing a touchscreen.
const char kSimulateTouchScreenWithMouse[]  =
    "simulate-touch-screen-with-mouse";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Experimentally enforces a one-site-per-process security policy.
// All cross-site navigations force process swaps, and we can restrict a
// renderer process's access rights based on its site.  For details, see:
// http://www.chromium.org/developers/design-documents/site-isolation
//
// Unlike --enable-strict-site-isolation (which allows cross-site iframes),
// this flag blocks cross-site documents even in iframes, until out-of-process
// iframe support is available.  Cross-site network requests do attach the
// normal set of cookies, but a renderer process is only allowed to view or
// modify cookies for its own site (via JavaScript).
// TODO(irobert): Implement the cross-site document blocking in
// http://crbug.com/159215.
const char kSitePerProcess[]                = "site-per-process";

// Skip gpu info collection, blacklist loading, and blacklist auto-update
// scheduling at browser startup time.
// Therefore, all GPU features are available, and about:gpu page shows empty
// content. The switch is intended only for tests.
const char kSkipGpuDataLoading[]            = "skip-gpu-data-loading";

// GestureTapDown events are deferred by this many miillseconds before
// sending them to the renderer.
const char kTapDownDeferralTimeMs[]         = "tap-down-deferral-time";

// Runs the security test for the renderer sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kTestingFixedHttpPort[]          = "testing-fixed-http-port";
const char kTestingFixedHttpsPort[]         = "testing-fixed-https-port";

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

// Prioritizes the UI's command stream in the GPU process
extern const char kUIPrioritizeInGpuProcess[] =
    "ui-prioritize-in-gpu-process";

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

// Causes the process to run as a worker subprocess.
const char kWorkerProcess[]                 = "worker";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

// Enables moving cursor by word in visual order.
const char kEnableVisualWordMovement[]      = "enable-visual-word-movement";

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

#if defined(OS_ANDROID)
// Disable history logging for media elements.
const char kDisableMediaHistoryLogging[]    = "disable-media-history";

// Disable user gesture requirement for media playback.
const char kDisableGestureRequirementForMediaPlayback[] =
    "disable-gesture-requirement-for-media-playback";

// Whether to run media elements in the renderer process.
const char kMediaPlayerInRenderProcess[]    = "media-player-in-render-process";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// Set to enable compatibility with legacy WebView synchronous APIs.
const char kEnableWebViewSynchronousAPIs[] = "enable-webview-synchronous-apis";
#endif

#if defined(OS_CHROMEOS)
// Disables panel fitting (used for mirror mode).
const char kDisablePanelFitting[]           = "disable-panel-fitting";
#endif

#if defined(OS_POSIX)
// Causes the child processes to cleanly exit via calling exit().
const char kChildCleanExit[]                = "child-clean-exit";
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
const char kDisableCarbonInterposing[]      = "disable-carbon-interposing";
#endif

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

#if defined(USE_AURA)
// Forces usage of the test compositor. Needed to run ui tests on bots.
extern const char kTestCompositor[]         = "test-compositor";
#endif

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[]              = "default-tile-width";
const char kDefaultTileHeight[]             = "default-tile-height";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerWidth[]          = "max-untiled-layer-width";
const char kMaxUntiledLayerHeight[]         = "max-untiled-layer-height";

// Use ExynosVideoDecodeAccelerator for video decode (instead of SECOMX)
const char kUseExynosVda[]                  = "use-exynos-vda";

const char kEnableFixedPositionCreatesStackingContext[]
    = "enable-fixed-position-creates-stacking-context";
const char kDisableFixedPositionCreatesStackingContext[]
    = "disable-fixed-position-creates-stacking-context";

// Defer image decoding in WebKit until painting.
const char kEnableDeferredImageDecoding[] = "enable-deferred-image-decoding";

// Disables history navigation in response to horizontal overscroll.
const char kDisableOverscrollHistoryNavigation[] =
    "disable-overscroll-history-navigation";

}  // namespace switches
