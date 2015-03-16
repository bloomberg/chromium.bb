// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switches.h"

namespace switches {

// The number of MSAA samples for canvas2D. Requires MSAA support by GPU to
// have an effect. 0 disables MSAA.
const char kAcceleratedCanvas2dMSAASampleCount[] = "canvas-msaa-sample-count";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Allows frames with an https origin to use WebSockets with an insecure URL
// (ws://).
const char kAllowInsecureWebSocketFromHttpsOrigin[] =
    "allow-insecure-websocket-from-https-origin";

// Allows loopback interface to be added in network list for peer connection.
const char kAllowLoopbackInPeerConnection[] =
    "allow-loopback-in-peer-connection";

// Enables the sandboxed processes to run without a job object assigned to them.
// This flag is required to allow Chrome to run in RemoteApps or Citrix. This
// flag can reduce the security of the sandboxed processes and allow them to do
// certain API calls like shut down Windows or access the clipboard. Also we
// lose the chance to kill some processes until the outer job that owns them
// finishes.
const char kAllowNoSandboxJob[]             = "allow-no-sandbox-job";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// The same as kAuditHandles except all handles are enumerated.
const char kAuditAllHandles[]               = "enable-handle-auditing-all";

// Enumerates and prints a child process' most dangerous handles when it
// is terminated.
const char kAuditHandles[]                  = "enable-handle-auditing";

// Choose which logging channels in blink platform to activate.  See
// Logging.cpp in blink's Source/platform for a list of available channels.
const char kBlinkPlatformLogChannels[]      = "blink-platform-log-channels";

// Block cross-site documents (i.e., HTML/XML/JSON) from being loaded in
// subresources when a document is not supposed to read them.  This will later
// allow us to block them from the entire renderer process when site isolation
// is enabled.
const char kBlockCrossSiteDocuments[]     = "block-cross-site-documents";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Dumps extra logging about plugin loading to the log file.
const char kDebugPluginLoading[] = "debug-plugin-loading";

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[]              = "default-tile-width";
const char kDefaultTileHeight[]             = "default-tile-height";

// Disable antialiasing on 2d canvas.
const char kDisable2dCanvasAntialiasing[]   = "disable-canvas-aa";

// Disables client-visible 3D APIs, in particular WebGL and Pepper 3D.
// This is controlled by policy and is kept separate from the other
// enable/disable switches to avoid accidentally regressing the policy
// support for controlling access to these APIs.
const char kDisable3DAPIs[]                 = "disable-3d-apis";

// Disable gpu-accelerated 2d canvas.
const char kDisableAccelerated2dCanvas[]    = "disable-accelerated-2d-canvas";

// Disables hardware acceleration of video decode, where available.
const char kDisableAcceleratedVideoDecode[] =
    "disable-accelerated-video-decode";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable one or more Blink runtime-enabled features.
// Use names from RuntimeEnabledFeatures.in, separated by commas.
// Applied after kEnableBlinkFeatures, and after other flags that change these
// features.
const char kDisableBlinkFeatures[]          = "disable-blink-features";

// Disable the Blink Scheduler. Ensures there's no reordering of blink tasks.
// This switch is intended only for performance tests.
const char kDisableBlinkScheduler[]         = "disable-blink-scheduler";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables delegated renderer.
const char kDisableDelegatedRenderer[]      = "disable-delegated-renderer";

// Handles URL requests by NPAPI plugins through the renderer.
const char kDisableDirectNPAPIRequests[]    = "disable-direct-npapi-requests";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
extern const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Prevent the compositor from using its GPU implementation.
const char kDisableGpuCompositing[]         = "disable-gpu-compositing";

// Disable the limit on the number of times the GPU process may be restarted
// This switch is intended only for tests.
extern const char kDisableGpuProcessCrashLimit[] =
    "disable-gpu-process-crash-limit";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization and kForceGpuRasterization flags.
const char kDisableGpuRasterization[]       = "disable-gpu-rasterization";

// When using CPU rasterizing disable low resolution tiling. This uses
// less power, particularly during animations, but more white may be seen
// during fast scrolling especially on slower devices.
const char kDisableLowResTiling[] = "disable-low-res-tiling";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[]            = "disable-gpu-watchdog";

// Suppresses hang monitor dialogs in renderer processes.  This may allow slow
// unload handlers on a page to prevent the tab from closing, but the Task
// Manager can be used to terminate the offending process in this case.
const char kDisableHangMonitor[]            = "disable-hang-monitor";

// Disable hiding the close buttons of inactive tabs when the tabstrip is in
// stacked mode.
const char kDisableHideInactiveStackedTabCloseButtons[] =
    "disable-hide-inactive-stacked-tab-close-buttons";

// Disable the RenderThread's HistogramCustomizer.
const char kDisableHistogramCustomizer[]    = "disable-histogram-customizer";

// Paint content on the main thread instead of the compositor thread.
const char kDisableImplSidePainting[]       = "disable-impl-side-painting";

// Prevent Java from running.
const char kDisableJava[]                   = "disable-java";

// Don't kill a child process when it sends a bad IPC message.  Apart
// from testing, it is a bad idea from a security perspective to enable
// this switch.
const char kDisableKillAfterBadIPC[]        = "disable-kill-after-bad-ipc";

// Disables prefixed Encrypted Media API (e.g. webkitGenerateKeyRequest()).
const char kDisablePrefixedEncryptedMedia[] =
    "disable-prefixed-encrypted-media";

// Disables LCD text.
const char kDisableLCDText[]                = "disable-lcd-text";

// Disables distance field text.
const char kDisableDistanceFieldText[]      = "disable-distance-field-text";

// Disable LocalStorage.
const char kDisableLocalStorage[]           = "disable-local-storage";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Disables Media Source API (i.e., the MediaSource object).
const char kDisableMediaSource[]            = "disable-media-source";

// Disables usage of the namespace sandbox.
const char kDisableNamespaceSandbox[]       = "disable-namespace-sandbox";

// Disables the Web Notification and the Push APIs.
const char kDisableNotifications[]          = "disable-notifications";

// Disable rasterizer that writes directly to GPU memory.
// Overrides the kEnableOneCopy flag.
const char kDisableOneCopy[]                = "disable-one-copy";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Disable discovering third-party plugins. Effectively loading only
// ones shipped with the browser plus third-party ones as specified by
// --extra-plugin-dir and --load-plugin switches.
const char kDisablePluginsDiscovery[]       = "disable-plugins-discovery";

// Taints all <canvas> elements, regardless of origin.
const char kDisableReadingFromCanvas[]      = "disable-reading-from-canvas";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[]   = "disable-seccomp-filter-sandbox";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[]          = "disable-setuid-sandbox";

// Disable shared workers.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// For tests, disable single thread scheduler and only manually composite.
const char kDisableSingleThreadProxyScheduler[] =
    "disable-single-thread-proxy-scheduler";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables SVG 1.1 DOM.
const char kDisableSVG1DOM[]                = "disable-svg1dom";

// Disable text blob rendering.
const char kDisableTextBlobs[]              = "disable-text-blobs";

// Disable multithreaded GPU compositing of web content.
const char kDisableThreadedCompositing[]     = "disable-threaded-compositing";

// Disable multithreaded, compositor scrolling of web content.
const char kDisableThreadedScrolling[]      = "disable-threaded-scrolling";

// Disable V8 idle tasks.
const char kDisableV8IdleTasks[]            = "disable-v8-idle-tasks";

// Don't enforce the same-origin policy. (Used by people testing their sites.)
const char kDisableWebSecurity[]            = "disable-web-security";

// Disables Blink's XSSAuditor. The XSSAuditor mitigates reflective XSS.
const char kDisableXSSAuditor[]             = "disable-xss-auditor";

// Specifies if the |DOMAutomationController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when automating dom
// based tests.
const char kDomAutomationController[]       = "dom-automation";

// Enable antialiasing on 2d canvas clips (as opposed to draw operations)
const char kEnable2dCanvasClipAntialiasing[] = "enable-2d-canvas-clip-aa";

// Disable partially decoding jpeg images using the GPU.
// At least YUV decoding will be accelerated when not using this flag.
// Has no effect unless GPU rasterization is enabled.
const char kDisableAcceleratedJpegDecoding[] =
    "disable-accelerated-jpeg-decoding";

// Enable bleeding-edge code to make Chrome draw content faster. The changes
// behind this path are very likely to break lots of content.
// ** DO NOT use this flag unless you know what you are doing. **
const char kEnableBleedingEdgeRenderingFastPaths[] =
    "enable-bleeding-edge-rendering-fast-paths";

// Enables LCD text.
const char kEnableLCDText[]                 = "enable-lcd-text";

// Enables using signed distance fields when rendering text.
// Only valid if GPU rasterization is enabled as well.
const char kEnableDistanceFieldText[]       = "enable-distance-field-text";

// Enable the experimental Credential Manager JavaScript API.
const char kEnableCredentialManagerAPI[]    = "enable-credential-manager-api";

// Use a BeginFrame signal from browser to renderer to schedule rendering.
const char kEnableBeginFrameScheduling[]    = "enable-begin-frame-scheduling";

// Enable the creation of compositing layers when it would prevent LCD text.
const char kEnablePreferCompositingToLCDText[] =
    "enable-prefer-compositing-to-lcd-text";

// Disable one or more Blink runtime-enabled features.
// Use names from RuntimeEnabledFeatures.in, separated by commas.
// Applied before kDisableBlinkFeatures, and after other flags that change these
// features.
const char kEnableBlinkFeatures[]           = "enable-blink-features";

// PlzNavigate: Use the experimental browser-side navigation path.
const char kEnableBrowserSideNavigation[]   = "enable-browser-side-navigation";

// Defer image decoding in WebKit until painting.
const char kEnableDeferredImageDecoding[]   = "enable-deferred-image-decoding";

// Enables Delay Agnostic AEC in WebRTC.
const char kEnableDelayAgnosticAec[]        = "enable-delay-agnostic-aec";

// Enables delegated renderer.
const char kEnableDelegatedRenderer[]       = "enable-delegated-renderer";

// Enables display list based 2d canvas implementation. Options:
//  1. Enable: allow browser to use display list for 2d canvas (browser makes
//     decision).
//  2. Force: browser always uses display list for 2d canvas.
const char kEnableDisplayList2dCanvas[]     = "enable-display-list-2d-canvas";
const char kForceDisplayList2dCanvas[]      = "force-display-list-2d-canvas";
const char kDisableDisplayList2dCanvas[]    = "disable-display-list-2d-canvas";

// Enables restarting interrupted downloads.
const char kEnableDownloadResumption[]      = "enable-download-resumption";

// Disables (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Enable experimental canvas features, e.g. canvas 2D context attributes
const char kEnableExperimentalCanvasFeatures[] =
    "enable-experimental-canvas-features";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// By default, cookies are not allowed on file://. They are needed for testing,
// for example page cycler and layout tests. See bug 1157243.
const char kEnableFileCookies[]             = "enable-file-cookies";

// Enables TRACE for GL calls in the renderer.
const char kEnableGpuClientTracing[]        = "enable-gpu-client-tracing";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGpuRasterization[]        = "enable-gpu-rasterization";

// When using CPU rasterizing generate low resolution tiling. Low res
// tiles may be displayed during fast scrolls especially on slower devices.
const char kEnableLowResTiling[] = "enable-low-res-tiling";

// Dynamically apply color profiles to web content images.
const char kEnableImageColorProfiles[]      = "enable-image-color-profiles";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// Enables the memory benchmarking extension
const char kEnableMemoryBenchmarking[]      = "enable-memory-benchmarking";

// Enables the network information API.
const char kEnableNetworkInformation[]      = "enable-network-information";

// Enable rasterizer that writes directly to GPU memory.
const char kEnableOneCopy[]                 = "enable-one-copy";

// Enables use of hardware overlay for fullscreen video playback. Android only.
const char kEnableOverlayFullscreenVideo[]  = "enable-overlay-fullscreen-video";

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Make the values returned to window.performance.memory more granular and more
// up to date in shared worker. Without this flag, the memory information is
// still available, but it is bucketized and updated less frequently. This flag
// also applys to workers.
const char kEnablePreciseMemoryInfo[] = "enable-precise-memory-info";

// Enables payloads for received push messages when using the W3C Push API.
const char kEnablePushMessagePayload[] = "enable-push-message-payload";

// Enable hasPermission() method of the W3C Push API.
const char kEnablePushMessagingHasPermission[] =
        "enable-push-messaging-has-permission";

// Set options to cache V8 data. (off, preparse data, or code)
const char kV8CacheOptions[] = "v8-cache-options";

// Enables the CSS multicol implementation that uses the regions implementation.
const char kEnableRegionBasedColumns[] =
    "enable-region-based-columns";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enables seccomp-bpf support for Android. Requires experimental kernel
// support. <http://crbug.com/166704>
const char kEnableSeccompFilterSandbox[] =
    "enable-seccomp-filter-sandbox";

// Enables the Skia benchmarking extension
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// Enables slimming paint: http://www.chromium.org/blink/slimming-paint
const char kEnableSlimmingPaint[]           = "enable-slimming-paint";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Enable spatial navigation
const char kEnableSpatialNavigation[]       = "enable-spatial-navigation";

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

// Blocks all insecure requests from secure contexts, and prevents the user
// from overriding that decision.
const char kEnableStrictMixedContentChecking[] =
    "enable-strict-mixed-content-checking";

// Blocks insecure usage of number of powerful features (geolocation, for
// example) that we haven't yet deprecated for the web at large.
const char kEnableStrictPowerfulFeatureRestrictions[] =
    "enable-strict-powerful-feature-restrictions";

// Enable support for sync events in ServiceWorkers.
const char kEnableServiceWorkerSync[]       = "enable-service-worker-sync";

// Enable use of experimental TCP sockets API for sending data in the
// SYN packet.
const char kEnableTcpFastOpen[]             = "enable-tcp-fastopen";

// Enabled threaded compositing for layout tests.
const char kEnableThreadedCompositing[]     = "enable-threaded-compositing";

// Enable tracing during the execution of browser tests.
const char kEnableTracing[]                 = "enable-tracing";

// The filename to write the output of the test tracing to.
const char kEnableTracingOutput[]           = "enable-tracing-output";

// Enable screen capturing support for MediaStream API.
const char kEnableUserMediaScreenCapturing[] =
    "enable-usermedia-screen-capturing";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enables the viewport meta tag, the de facto way to control layout which works
// only on mobile browsers.
const char kEnableViewportMeta[]            = "enable-viewport-meta";

// Resizes of the main frame are the caused by changing between landscape
// and portrait mode (i.e. Android) so the page should be rescaled to fit
const char kMainFrameResizesAreOrientationChanges[] =
    "main-frame-resizes-are-orientation-changes";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables WebGL rendering into a scanout buffer for overlay support.
const char kEnableWebGLImageChromium[] = "enable-webgl-image-chromium";

// Enables Web MIDI API.
const char kEnableWebMIDI[]                 = "enable-web-midi";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[]                = "enable-zero-copy";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// This option can be used to force field trials when testing changes locally.
// The argument is a list of name and value pairs, separated by slashes. If a
// trial name is prefixed with an asterisk, that trial will start activated.
// For example, the following argument defines two trials, with the second one
// activated: "GoogleNow/Enable/*MaterialDesignNTP/Default/"
// This option is also used by the browser to send the list of trials to
// renderers, using the same format. See
// FieldTrialList::CreateTrialsFromString() in field_trial.h for details.
const char kForceFieldTrials[]              = "force-fieldtrials";

// Always use the Skia GPU backend for drawing layer tiles. Only valid with GPU
// accelerated compositing + impl-side painting. Overrides the
// kEnableGpuRasterization flag.
const char kForceGpuRasterization[]        = "force-gpu-rasterization";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Enable threaded GPU rasterization.
const char kEnableThreadedGpuRasterization[] =
    "enable-threaded-gpu-rasterization";

// Disable threaded GPU rasterization.  Overrides enable.
const char kDisableThreadedGpuRasterization[] =
    "disable-threaded-gpu-rasterization";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// Always use text blob rendering, overriding kDisableTextBlobs and any
// heuristics that may otherwise disable it.
// TODO(fmalita): remove after --disable-impl-side-painting is phased out.
const char kForceTextBlobs[]                = "force-text-blobs";

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

// Allows shmat() system call in the GPU sandbox.
const char kGpuSandboxAllowSysVShm[]        = "gpu-sandbox-allow-sysv-shm";

// Makes GPU sandbox failures fatal.
const char kGpuSandboxFailuresFatal[]       = "gpu-sandbox-failures-fatal";

// Starts the GPU sandbox before creating a GL context.
const char kGpuSandboxStartEarly[]          = "gpu-sandbox-start-early";

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

// Overrides the timeout, in seconds, that a child process waits for a
// connection from the browser before killing itself.
const char kIPCConnectionTimeout[]          = "ipc-connection-timeout";

// Specifies the flags passed to JS engine
const char kJavaScriptFlags[]               = "js-flags";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Logs GPU control list decisions when enforcing blacklist rules.
const char kLogGpuControlListDecisions[]    = "log-gpu-control-list-decisions";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Enables saving net log events to a file and sets the file name to use.
const char kLogNetLog[]                     = "log-net-log";

// Make plugin processes log their sent and received messages to VLOG(1).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[]         = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[]          = "max-untiled-layer-width";

// Sample memory usage with high frequency and store the results to the
// Renderer.Memory histogram. Used in memory tests.
const char kMemoryMetrics[]                 = "memory-metrics";

// Mutes audio sent to the audio device so it is not audible during
// automated testing.
const char kMuteAudio[]                     = "mute-audio";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

// Disables the sandbox for all process types that are normally sandboxed.
const char kNoSandbox[]                     = "no-sandbox";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[]              = "num-raster-threads";

// Controls the behavior of history navigation in response to horizontal
// overscroll.
// Set the value to '0' to disable.
// Set the value to '1' to enable the behavior where pages slide in and out in
// response to the horizontal overscroll gesture and a screenshot of the target
// page is shown.
// Set the value to '2' to enable the simplified overscroll UI where a
// navigation arrow slides in from the side of the screen in response to the
// horizontal overscroll gesture.
// Defaults to '1'.
const char kOverscrollHistoryNavigation[] =
    "overscroll-history-navigation";

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

// Enables more web features over insecure connections. Designed to be used
// for testing purposes only.
const char kReduceSecurityForTesting[]      = "reduce-security-for-testing";

// Register Pepper plugins (see pepper_plugin_list.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// Enables remote debug over HTTP on the specified port.
const char kRemoteDebuggingPort[]           = "remote-debugging-port";

// The contents of this flag are prepended to the renderer command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Overrides the default/calculated limit to the number of renderer processes.
// Very high values for this setting can lead to high memory/resource usage
// or instability.
const char kRendererProcessLimit[]          = "renderer-process-limit";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Reduce the default `referer` header's granularity.
const char kReducedReferrerGranularity[] =
  "reduced-referrer-granularity";

// Handles frame scrolls via the root RenderLayer instead of the FrameView.
const char kRootLayerScrolls[]              = "root-layer-scrolls";

// Causes the process to run as a sandbox IPC subprocess.
const char kSandboxIPCProcess[]             = "sandbox-ipc";

// Enables or disables scroll end effect in response to vertical overscroll.
// Set the value to '1' to enable the feature, and set to '0' to disable.
// Defaults to disabled.
const char kScrollEndEffect[] = "scroll-end-effect";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[]                = "show-paint-rects";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Experimentally enforces a one-site-per-process security policy.
// All cross-site navigations force process swaps, and we can restrict a
// renderer process's access rights based on its site.  For details, see:
// http://www.chromium.org/developers/design-documents/site-isolation
//
// Unlike --enable-strict-site-isolation (which allows cross-site iframes),
// this flag does not affect which cookies are attached to cross-site requests.
// Support is being added to render cross-site iframes in a different process
// than their parent pages.
const char kSitePerProcess[]                = "site-per-process";

// Skip gpu info collection, blacklist loading, and blacklist auto-update
// scheduling at browser startup time.
// Therefore, all GPU features are available, and about:gpu page shows empty
// content. The switch is intended only for layout tests.
// TODO(gab): Get rid of this switch entirely.
const char kSkipGpuDataLoading[]            = "skip-gpu-data-loading";

// Specifies if the browser should start in fullscreen mode, like if the user
// had pressed F11 right after startup.
const char kStartFullscreen[] = "start-fullscreen";

// Specifies if the |StatsCollectionController| needs to be bound in the
// renderer. This binding happens on per-frame basis and hence can potentially
// be a performance bottleneck. One should only enable it when running a test
// that needs to access the provided statistics.
const char kStatsCollectionController[] =
    "enable-stats-collection-bindings";

// Upscale defaults to "good".
const char kTabCaptureDownscaleQuality[]    = "tab-capture-downscale-quality";

// Scaling quality for capturing tab. Should be one of "fast", "good" or "best".
// One flag for upscaling, one for downscaling.
// Upscale defaults to "best".
const char kTabCaptureUpscaleQuality[]      = "tab-capture-upscale-quality";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kTestingFixedHttpPort[]          = "testing-fixed-http-port";
const char kTestingFixedHttpsPort[]         = "testing-fixed-https-port";

// Type of the current test harness ("browser" or "ui").
const char kTestType[]                      = "test-type";

// Causes TRACE_EVENT flags to be recorded beginning with shutdown. Optionally,
// can specify the specific trace categories to include (e.g.
// --trace-shutdown=base,net) otherwise, all events are recorded.
// --trace-shutdown-file can be used to control where the trace log gets stored
// to since there is otherwise no way to access the result.
const char kTraceShutdown[]                 = "trace-shutdown";

// If supplied, sets the file which shutdown tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-shutdown is also supplied.
// Example: --trace-shutdown --trace-shutdown-file=/tmp/trace_event.log
const char kTraceShutdownFile[]             = "trace-shutdown-file";

// Causes TRACE_EVENT flags to be recorded from startup. Optionally, can
// specify the specific trace categories to include (e.g.
// --trace-startup=base,net) otherwise, all events are recorded. Setting this
// flag results in the first call to BeginTracing() to receive all trace events
// since startup. In Chrome, you may find --trace-startup-file and
// --trace-startup-duration to control the auto-saving of the trace (not
// supported in the base-only TraceLog component).
const char kTraceStartup[]                  = "trace-startup";

// Sets the time in seconds until startup tracing ends. If omitted a default of
// 5 seconds is used. Has no effect without --trace-startup, or if
// --startup-trace-file=none was supplied.
const char kTraceStartupDuration[]          = "trace-startup-duration";

// If supplied, sets the file which startup tracing will be stored into, if
// omitted the default will be used "chrometrace.log" in the current directory.
// Has no effect unless --trace-startup is also supplied.
// Example: --trace-startup --trace-startup-file=/tmp/trace_event.log
// As a special case, can be set to 'none' - this disables automatically saving
// the result to a file and the first manually recorded trace will then receive
// all events since startup.
const char kTraceStartupFile[]              = "trace-startup-file";

// Sets the target URL for uploading tracing data.
const char kTraceUploadURL[]                = "trace-upload-url";


// Prioritizes the UI's command stream in the GPU process
extern const char kUIPrioritizeInGpuProcess[] =
    "ui-prioritize-in-gpu-process";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

// Enable native GPU memory buffer support when available.
const char kEnableNativeGpuMemoryBuffers[] = "enable-native-gpu-memory-buffers";

// Overrides the default texture target used with CHROMIUM_image extension.
const char kUseImageTextureTarget[] = "use-image-texture-target";

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

// Use normal priority for tile task worker threads.  Otherwise they may
// be run at background priority on some platforms.
const char kUseNormalPriorityForTileTaskWorkerThreads[] =
    "use-normal-priority-for-tile-task-worker-threads";

// Use the new surfaces system to handle compositor delegation.
const char kUseSurfaces[] = "use-surfaces";

// Disable the use of the new surfaces system to handle compositor delegation.
const char kDisableSurfaces[] = "disable-surfaces";

// The contents of this flag are prepended to the utility process command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

// Allows MDns to access network in sandboxed process.
const char kUtilityProcessEnableMDns[]      = "utility-enable-mdns";

const char kUtilityProcessRunningElevated[] = "utility-run-elevated";

// In debug builds, asserts that the stream of input events is valid.
const char kValidateInputEventStream[] = "validate-input-event-stream";

// Will add kWaitForDebugger to every child processes. If a value is passed, it
// will be used as a filter to determine if the child process should have the
// kWaitForDebugger flag passed on or not.
const char kWaitForDebuggerChildren[]       = "wait-for-debugger-children";

// The prefix used when starting the zygote process. (i.e. 'gdb --args')
const char kZygoteCmdPrefix[]               = "zygote-cmd-prefix";

// Causes the process to run as a renderer zygote.
const char kZygoteProcess[]                 = "zygote";

#if defined(ENABLE_WEBRTC)
// Disables HW decode acceleration for WebRTC.
const char kDisableWebRtcHWDecoding[]       = "disable-webrtc-hw-decoding";

// Disables encryption of RTP Media for WebRTC. When Chrome embeds Content, it
// ignores this switch on its stable and beta channels.
const char kDisableWebRtcEncryption[]      = "disable-webrtc-encryption";

// Disables HW encode acceleration for WebRTC.
const char kDisableWebRtcHWEncoding[]       = "disable-webrtc-hw-encoding";

// Enables H264 HW encode acceleration for WebRTC.
const char kEnableWebRtcHWH264Encoding[]    = "enable-webrtc-hw-h264-encoding";

// Enables Origin header in Stun messages for WebRTC.
const char kEnableWebRtcStunOrigin[]        = "enable-webrtc-stun-origin";

// Override the maximum framerate as can be specified in calls to getUserMedia.
// This flag expects a value.  Example: --max-gum-fps=17.5
const char kWebRtcMaxCaptureFramerate[]     = "max-gum-fps";

#if defined(OS_LINUX) || defined(OS_MACOSX)
// Enables Video Capture Device captured data uploaded to Texture for zero-copy
// capture pipeline.
const char kEnableWebRtcCaptureToTexture[] = "enable-webrtc-capture-to-texture";
#endif
#endif

#if defined(OS_ANDROID)
// Disable user gesture requirement for media playback.
const char kDisableGestureRequirementForMediaPlayback[] =
    "disable-gesture-requirement-for-media-playback";

// Disable the click delay by sending click events during double tap.
const char kDisableClickDelay[]             = "disable-click-delay";

// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[]   = "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
const char kDisablePullToRefreshEffect[]   = "disable-pull-to-refresh-effect";

// WebRTC is enabled by default on Android.
const char kDisableWebRTC[]                 = "disable-webrtc";

// Always use the video overlay for the embedded video.
// This switch is intended only for tests.
const char kForceUseOverlayEmbeddedVideo[] = "force-use-overlay-embedded-video";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";

// Block ChildProcessMain thread of the renderer's ChildProcessService until a
// Java debugger is attached.
const char kRendererWaitForJavaDebugger[] = "renderer-wait-for-java-debugger";
#endif

// Disable web audio API.
const char kDisableWebAudio[]               = "disable-webaudio";

#if defined(OS_CHROMEOS)
// Disables panel fitting (used for mirror mode).
const char kDisablePanelFitting[]           = "disable-panel-fitting";

// Disables VA-API accelerated video encode.
const char kDisableVaapiAcceleratedVideoEncode[] =
    "disable-vaapi-accelerated-video-encode";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Allows sending text-to-speech requests to speech-dispatcher, a common
// Linux speech service. Because it's buggy, the user must explicitly
// enable it so that visiting a random webpage can't cause instability.
const char kEnableSpeechDispatcher[] = "enable-speech-dispatcher";
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Disables support for Core Animation plugins. This is triggered when
// accelerated compositing is disabled. See http://crbug.com/122430.
const char kDisableCoreAnimationPlugins[] =
    "disable-core-animation-plugins";
#endif

#if defined(OS_WIN)
// Device scale factor passed to certain processes like renderers, etc.
const char kDeviceScaleFactor[]     = "device-scale-factor";

// Disable the Legacy Window which corresponds to the size of the WebContents.
const char kDisableLegacyIntermediateWindow[] = "disable-legacy-window";

// Enables or disables the Win32K process mitigation policy for renderer
// processes which prevents them from invoking user32 and gdi32 system calls
// which enter the kernel. This is only supported on Windows 8 and beyond.
const char kDisableWin32kRendererLockDown[] =
    "disable-win32k-renderer-lockdown";
const char kEnableWin32kRendererLockDown[] =
    "enable-win32k-renderer-lockdown";

// DirectWrite FontCache is shared by browser to renderers using shared memory.
// This switch allows specifying suffix to shared memory section name to avoid
// clashes between different instances of Chrome.
const char kFontCacheSharedMemSuffix[] = "font-cache-shared-mem-suffix";
#endif

// Enables the use of NPAPI plugins.
const char kEnableNpapi[]                   = "enable-npapi";

#if defined(ENABLE_PLUGINS)
// Enables the plugin power saver feature.
const char kEnablePluginPowerSaver[] = "enable-plugin-power-saver";
#endif

// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
