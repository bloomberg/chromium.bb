// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_switches.h"

namespace switches {

// The number of MSAA samples for canvas2D. Requires MSAA support by GPU to
// have an effect. 0 disables MSAA.
const char kAcceleratedCanvas2dMSAASampleCount[] = "canvas-msaa-sample-count";

// Override the default minimum starting volume of the Automatic Gain Control
// algorithm in WebRTC used with audio tracks from getUserMedia.
// The valid range is 12-255. Values outside that range will be clamped
// to the lowest or highest valid value inside WebRTC.
// TODO(tommi): Remove this switch when crbug.com/555577 is fixed.
const char kAgcStartupMinVolume[] = "agc-startup-min-volume";

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

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

// Choose which logging channels in blink platform to activate.  See
// Logging.cpp in blink's Source/platform for a list of available channels.
const char kBlinkPlatformLogChannels[]      = "blink-platform-log-channels";

// Set blink settings. Format is <name>[=<value],<name>[=<value>],...
// The names are declared in Settings.in. For boolean type, use "true", "false",
// or omit '=<value>' part to set to true. For enum type, use the int value of
// the enum value. Applied after other command line flags and prefs.
const char kBlinkSettings[]                 = "blink-settings";

// Causes the browser process to crash on startup.
const char kBrowserCrashTest[]              = "crash-test";

// Path to the exe to run for the renderer and plugin subprocesses.
const char kBrowserSubprocessPath[]         = "browser-subprocess-path";

// Causes the implementations of guests (inner WebContents) to use
// out-of-process iframes.
const char kUseCrossProcessFramesForGuests[]   =
    "use-cross-process-frames-for-guests";

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

// Disable hardware acceleration of mjpeg decode for captured frame, where
// available.
const char kDisableAcceleratedMjpegDecode[] =
    "disable-accelerated-mjpeg-decode";

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

// Disables new cc/animation system (Project Heaviside). crbug.com/394772
const char kDisableCompositorAnimationTimelines[] =
    "disable-compositor-animation-timelines";

// Disables HTML5 DB support.
const char kDisableDatabases[]              = "disable-databases";

// Disables Delay Agnostic AEC in WebRTC.
const char kDisableDelayAgnosticAec[]       = "disable-delay-agnostic-aec";

// Handles URL requests by NPAPI plugins through the renderer.
const char kDisableDirectNPAPIRequests[]    = "disable-direct-npapi-requests";

// Disable the per-domain blocking for 3D APIs after GPU reset.
// This switch is intended only for tests.
const char kDisableDomainBlockingFor3DAPIs[] =
    "disable-domain-blocking-for-3d-apis";

// Disable experimental WebGL support.
const char kDisableExperimentalWebGL[]      = "disable-webgl";

// Comma-separated list of feature names to disable. See also kEnableFeatures.
const char kDisableFeatures[]               = "disable-features";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Disable 3D inside of flapper.
const char kDisableFlash3d[]                = "disable-flash-3d";

// Disable Stage3D inside of flapper.
const char kDisableFlashStage3d[]           = "disable-flash-stage3d";

// Disable user gesture requirement for media playback.
const char kDisableGestureRequirementForMediaPlayback[] =
    "disable-gesture-requirement-for-media-playback";

// Disables GPU hardware acceleration.  If software renderer is not in place,
// then the GPU process won't launch.
const char kDisableGpu[]                    = "disable-gpu";

// Prevent the compositor from using its GPU implementation.
const char kDisableGpuCompositing[]         = "disable-gpu-compositing";

// Disable proactive early init of GPU process.
const char kDisableGpuEarlyInit[]           = "disable-gpu-early-init";

// Do not force that all compositor resources be backed by GPU memory buffers.
const char kDisableGpuMemoryBufferCompositorResources[] =
    "disable-gpu-memory-buffer-compositor-resources";

// Disable GpuMemoryBuffer backed VideoFrames.
const char kDisableGpuMemoryBufferVideoFrames[] =
    "disable-gpu-memory-buffer-video-frames";

// Disable the limit on the number of times the GPU process may be restarted
// This switch is intended only for tests.
const char kDisableGpuProcessCrashLimit[] = "disable-gpu-process-crash-limit";

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

// Don't kill a child process when it sends a bad IPC message.  Apart
// from testing, it is a bad idea from a security perspective to enable
// this switch.
const char kDisableKillAfterBadIPC[]        = "disable-kill-after-bad-ipc";

// Enables prefixed Encrypted Media API (e.g. webkitGenerateKeyRequest()).
const char kEnablePrefixedEncryptedMedia[] =
    "enable-prefixed-encrypted-media";

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

// Disables native GPU memory buffer support.
const char kDisableNativeGpuMemoryBuffers[] =
    "disable-native-gpu-memory-buffers";

// Disables the Web Notification and the Push APIs.
const char kDisableNotifications[]          = "disable-notifications";

// Disable Pepper3D.
const char kDisablePepper3d[]               = "disable-pepper-3d";

// Disables the Permissions API.
const char kDisablePermissionsAPI[]         = "disable-permissions-api";

// Disables compositor-accelerated touch-screen pinch gestures.
const char kDisablePinch[]                  = "disable-pinch";

// Disable discovering third-party plugins. Effectively loading only
// ones shipped with the browser plus third-party ones as specified by
// --extra-plugin-dir and --load-plugin switches.
const char kDisablePluginsDiscovery[]       = "disable-plugins-discovery";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables the Presentation API.
const char kDisablePresentationAPI[]        = "disable-presentation-api";

// Disables RGBA_4444 textures.
const char kDisableRGBA4444Textures[]       = "disable-rgba-4444-textures";

// Taints all <canvas> elements, regardless of origin.
const char kDisableReadingFromCanvas[]      = "disable-reading-from-canvas";

// Disables remote web font support. SVG font should always work whether this
// option is specified or not.
const char kDisableRemoteFonts[]            = "disable-remote-fonts";

// Turns off the accessibility in the renderer.
const char kDisableRendererAccessibility[]  = "disable-renderer-accessibility";

// Prevent renderer process backgrounding when set.
const char kDisableRendererBackgrounding[]  = "disable-renderer-backgrounding";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[]   = "disable-seccomp-filter-sandbox";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[]          = "disable-setuid-sandbox";

// Disable shared workers.
const char kDisableSharedWorkers[]          = "disable-shared-workers";

// Disable smooth scrolling for testing.
const char kDisableSmoothScrolling[]        = "disable-smooth-scrolling";

// Disables the use of a 3D software rasterizer.
const char kDisableSoftwareRasterizer[]     = "disable-software-rasterizer";

// Disables the Web Speech API.
const char kDisableSpeechAPI[]              = "disable-speech-api";

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

// Disable rasterizer that writes directly to GPU memory associated with tiles.
const char kDisableZeroCopy[]                = "disable-zero-copy";

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

// Enables LCD text.
const char kEnableLCDText[]                 = "enable-lcd-text";

// Enables using signed distance fields when rendering text.
// Only valid if GPU rasterization is enabled as well.
const char kEnableDistanceFieldText[]       = "enable-distance-field-text";

// Enable the experimental Credential Manager JavaScript API.
const char kEnableCredentialManagerAPI[]    = "enable-credential-manager-api";

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

// Enables display list based 2d canvas implementation. Options:
//  1. Enable: allow browser to use display list for 2d canvas (browser makes
//     decision).
//  2. Force: browser always uses display list for 2d canvas.
const char kEnableDisplayList2dCanvas[]     = "enable-display-list-2d-canvas";
const char kForceDisplayList2dCanvas[]      = "force-display-list-2d-canvas";
const char kDisableDisplayList2dCanvas[]    = "disable-display-list-2d-canvas";

// Disables (unprefixed) Encrypted Media Extensions.
const char kDisableEncryptedMedia[] = "disable-encrypted-media";

// Enable experimental canvas features, e.g. canvas 2D context attributes
const char kEnableExperimentalCanvasFeatures[] =
    "enable-experimental-canvas-features";

// Enables Web Platform features that are in development.
const char kEnableExperimentalWebPlatformFeatures[] =
    "enable-experimental-web-platform-features";

// Comma-separated list of feature names to enable. See also kDisableFeatures.
const char kEnableFeatures[] = "enable-features";

// Enable Web Bluetooth.
const char kEnableWebBluetooth[] = "enable-web-bluetooth";

// Enables TRACE for GL calls in the renderer.
const char kEnableGpuClientTracing[]        = "enable-gpu-client-tracing";

// Specify that all compositor resources should be backed by GPU memory buffers.
const char kEnableGpuMemoryBufferCompositorResources[] =
    "enable-gpu-memory-buffer-compositor-resources";

// Enable GpuMemoryBuffer backed VideoFrames.
const char kEnableGpuMemoryBufferVideoFrames[] =
    "enable-gpu-memory-buffer-video-frames";

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

// Enable the Mojo shell connection in renderers.
const char kEnableMojoShellConnection[] = "enable-mojo-shell-connection";

// Enables the network information API.
const char kEnableNetworkInformation[]      = "enable-network-information";

// Enables non-validating reload on reload-to-refresh-content (e.g. pull-
// to-refresh).
const char kEnableNonValidatingReloadOnRefreshContent[] =
    "enable-non-validating-reload-on-refresh-content";

// Enables partial raster. Enabling this switch also enables the use of
// persistent gpu memory buffers.
const char kEnablePartialRaster[] = "enable-partial-raster";

// Enables compositor-accelerated touch-screen pinch gestures.
const char kEnablePinch[]                   = "enable-pinch";

// Enables testing features of the Plugin Placeholder. For internal use only.
const char kEnablePluginPlaceholderTesting[] =
    "enable-plugin-placeholder-testing";

// Make the values returned to window.performance.memory more granular and more
// up to date in shared worker. Without this flag, the memory information is
// still available, but it is bucketized and updated less frequently. This flag
// also applys to workers.
const char kEnablePreciseMemoryInfo[] = "enable-precise-memory-info";

// Enables RGBA_4444 textures.
const char kEnableRGBA4444Textures[] = "enable-rgba-4444-textures";

// Set options to cache V8 data. (off, preparse data, or code)
const char kV8CacheOptions[] = "v8-cache-options";

// Signals that the V8 natives file has been transfered to the child process
// by a file descriptor.
const char kV8NativesPassedByFD[] = "v8-natives-passed-by-fd";

// Signals that the V8 startup snapshot file has been transfered to the child
// process by a file descriptor.
const char kV8SnapshotPassedByFD[] = "v8-snapshot-passed-by-fd";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
const char kEnableScrollAnchoring[]         = "enable-scroll-anchoring";

// Enables the Skia benchmarking extension
const char kEnableSkiaBenchmarking[]        = "enable-skia-benchmarking";

// Enables slimming paint phase 2: http://www.chromium.org/blink/slimming-paint
const char kEnableSlimmingPaintV2[]         = "enable-slimming-paint-v2";

// On platforms that support it, enables smooth scroll animation.
const char kEnableSmoothScrolling[]         = "enable-smooth-scrolling";

// Enable spatial navigation
const char kEnableSpatialNavigation[]       = "enable-spatial-navigation";

// Enables implementation of the Cache-Control: stale-while-revalidate directive
// which permits servers to allow the use of stale resources while revalidation
// proceeds in the background.
const char kEnableStaleWhileRevalidate[]    = "enable-stale-while-revalidate";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Blocks all insecure requests from secure contexts, and prevents the user
// from overriding that decision.
const char kEnableStrictMixedContentChecking[] =
    "enable-strict-mixed-content-checking";

// Blocks insecure usage of a number of powerful features (device orientation,
// for example) that we haven't yet deprecated for the web at large.
const char kEnableStrictPowerfulFeatureRestrictions[] =
    "enable-strict-powerful-feature-restrictions";

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

// Enable the mode that uses zooming to implment device scale factor behavior.
const char kEnableUseZoomForDSF[]            = "enable-use-zoom-for-dsf";

// Enables the use of the @viewport CSS rule, which allows
// pages to control aspects of their own layout. This also turns on touch-screen
// pinch gestures.
const char kEnableViewport[]                = "enable-viewport";

// Enable the Vtune profiler support.
const char kEnableVtune[]                   = "enable-vtune-support";

// Enables WebGL extensions not yet approved by the community.
const char kEnableWebGLDraftExtensions[] = "enable-webgl-draft-extensions";

// Enables WebGL rendering into a scanout buffer for overlay support.
const char kEnableWebGLImageChromium[] = "enable-webgl-image-chromium";

// Enables interaction with virtual reality devices.
const char kEnableWebVR[] = "enable-webvr";

// Enables gesture generation for wheel events.
const char kEnableWheelGestures[] = "enable-wheel-gestures";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[]                = "enable-zero-copy";

// Explicitly allows additional ports using a comma-separated list of port
// numbers.
const char kExplicitlyAllowedPorts[]        = "explicitly-allowed-ports";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// Always use the Skia GPU backend for drawing layer tiles. Only valid with GPU
// accelerated compositing + impl-side painting. Overrides the
// kEnableGpuRasterization flag.
const char kForceGpuRasterization[]        = "force-gpu-rasterization";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Forces use of hardware overlay for fullscreen video playback. Useful for
// testing the Android overlay fullscreen functionality on other platforms.
const char kForceOverlayFullscreenVideo[]   = "force-overlay-fullscreen-video";

// Force renderer accessibility to be on instead of enabling it on demand when
// a screen reader is detected. The disable-renderer-accessibility switch
// overrides this if present.
const char kForceRendererAccessibility[]    = "force-renderer-accessibility";

// For development / testing only. When running content_browsertests,
// saves output of failing accessibility tests to their expectations files in
// content/test/data/accessibility/, overwriting existing file content.
const char kGenerateAccessibilityTestExpectations[] =
    "generate-accessibility-test-expectations";

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

// Resizes of the main frame are caused by changing between landscape and
// portrait mode (i.e. Android) so the page should be rescaled to fit.
const char kMainFrameResizesAreOrientationChanges[] =
    "main-frame-resizes-are-orientation-changes";

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

// Enable or disable appcontainer/lowbox for renderer on Win8+ platforms.
const char kEnableAppContainer[]           = "enable-appcontainer";
const char kDisableAppContainer[]          = "disable-appcontainer";

// Number of worker threads used to rasterize content.
const char kNumRasterThreads[]              = "num-raster-threads";

// Override the behavior of plugin throttling for testing.
// By default the throttler is only enabled for a hard-coded list of plugins.
// Set the value to 'ignore-list' to ignore the hard-coded list.
// Set the value to 'always' to always throttle every plugin instance.
const char kOverridePluginPowerSaverForTesting[] =
    "override-plugin-power-saver-for-testing";

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

// Enforces a one-site-per-process security policy:
//  * Each renderer process, for its whole lifetime, is dedicated to rendering
//    pages for just one site.
//  * Thus, pages from different sites are never in the same process.
//  * A renderer process's access rights are restricted based on its site.
//  * All cross-site navigations force process swaps.
//  * <iframe>s are rendered out-of-process whenever the src= is cross-site.
//
// More details here:
// http://www.chromium.org/developers/design-documents/site-isolation
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

// Controls how text selection granularity changes when touch text selection
// handles are dragged. Should be "character" or "direction". If not specified,
// the platform default is used.
const char kTouchTextSelectionStrategy[]    = "touch-selection-strategy";

// Prioritizes the UI's command stream in the GPU process
const char kUIPrioritizeInGpuProcess[] = "ui-prioritize-in-gpu-process";

// Bypass the media stream infobar by selecting the default device for media
// streams (e.g. WebRTC). Works with --use-fake-device-for-media-stream.
const char kUseFakeUIForMediaStream[]     = "use-fake-ui-for-media-stream";

// Use the Mandoline UI Service in the Chrome render process.
const char kUseMusInRenderer[] = "use-mus-in-renderer";

// Enable native GPU memory buffer support when available.
const char kEnableNativeGpuMemoryBuffers[] = "enable-native-gpu-memory-buffers";

// Texture target for CHROMIUM_image backed content textures.
const char kContentImageTextureTarget[] = "content-image-texture-target";

// Texture target for CHROMIUM_image backed video frame textures.
const char kVideoImageTextureTarget[] = "video-image-texture-target";

// Set when Chromium should use a mobile user agent.
const char kUseMobileUserAgent[] = "use-mobile-user-agent";

// Use normal priority for tile task worker threads.  Otherwise they may
// be run at background priority on some platforms.
const char kUseNormalPriorityForTileTaskWorkerThreads[] =
    "use-normal-priority-for-tile-task-worker-threads";

// Use remote compositor for the renderer.
const char kUseRemoteCompositing[] = "use-remote-compositing";

// The contents of this flag are prepended to the utility process command line.
// Useful values might be "valgrind" or "xterm -e gdb --args".
const char kUtilityCmdPrefix[]              = "utility-cmd-prefix";

// Causes the process to run as a utility subprocess.
const char kUtilityProcess[]                = "utility";

// The utility process is sandboxed, with access to one directory. This flag
// specifies the directory that can be accessed.
const char kUtilityProcessAllowedDir[]      = "utility-allowed-dir";

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

// Enables negotiation of DTLS 1.2 for WebRTC.
const char kEnableWebRtcDtls12[]            = "enable-webrtc-dtls12";

// Enables H264 HW encode acceleration for WebRTC.
const char kEnableWebRtcHWH264Encoding[]    = "enable-webrtc-hw-h264-encoding";

// Enables Origin header in Stun messages for WebRTC.
const char kEnableWebRtcStunOrigin[]        = "enable-webrtc-stun-origin";

// Enforce IP Permission check. TODO(guoweis): Remove this once the feature is
// not under finch and becomes the default.
const char kEnforceWebRtcIPPermissionCheck[] =
    "enforce-webrtc-ip-permission-check";

// Override WebRTC IP handling policy to mimic the behavior when WebRTC IP
// handling policy is specified in Preferences.
const char kForceWebRtcIPHandlingPolicy[] = "force-webrtc-ip-handling-policy";

// Renderer process parameter for WebRTC Stun probe trial to determine the
// interval. Please see SetupStunProbeTrial in
// chrome_browser_field_trials_desktop.cc for more detail.
const char kWebRtcStunProbeTrialParameter[] = "webrtc-stun-probe-trial";

// Override the maximum framerate as can be specified in calls to getUserMedia.
// This flag expects a value.  Example: --max-gum-fps=17.5
const char kWebRtcMaxCaptureFramerate[]     = "max-gum-fps";
#endif

#if defined(OS_ANDROID)
// Disable external animation system for Android compositor.
// See also kDisableCompositorAnimationTimelines for renderer compositors.
const char kDisableAndroidCompositorAnimationTimelines[] =
    "disable-android-compositor-animation-timelines";

// Disable overscroll edge effects like those found in Android views.
const char kDisableOverscrollEdgeEffect[]   = "disable-overscroll-edge-effect";

// Disable the pull-to-refresh effect when vertically overscrolling content.
const char kDisablePullToRefreshEffect[]   = "disable-pull-to-refresh-effect";

// Disable the locking feature of the screen orientation API.
const char kDisableScreenOrientationLock[]  = "disable-screen-orientation-lock";

// Enable inverting of selection handles so that they are not clipped by the
// viewport boundaries.
const char kEnableAdaptiveSelectionHandleOrientation[] =
    "enable-adaptive-selection-handle-orientation";

// Enable drag manipulation of longpress-triggered text selections.
const char kEnableLongpressDragSelection[]  = "enable-longpress-drag-selection";

// Enable IPC-based synchronous compositing. Used by Android WebView.
const char kIPCSyncCompositing[]            = "ipc-sync-compositing";

// The telephony region (ISO country code) to use in phone number detection.
const char kNetworkCountryIso[] = "network-country-iso";

// Enables remote debug over HTTP on the specified socket name.
const char kRemoteDebuggingSocketName[]     = "remote-debugging-socket-name";

// Block ChildProcessMain thread of the renderer's ChildProcessService until a
// Java debugger is attached.
const char kRendererWaitForJavaDebugger[] = "renderer-wait-for-java-debugger";
#endif

// Enable the aggressive flushing of DOM Storage to minimize data loss.
const char kEnableAggressiveDOMStorageFlushing[] =
    "enable-aggressive-domstorage-flushing";

// Disable web audio API.
const char kDisableWebAudio[]               = "disable-webaudio";

// Enable audio for desktop share.
const char kEnableAudioSupportForDesktopShare[] =
    "enable-audio-support-for-desktop-share";

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

// Disables the Win32K process mitigation policy for renderer processes.
const char kDisableWin32kRendererLockDown[] =
    "disable-win32k-renderer-lockdown";

// Enables the Win32K process mitigation policy for certain PPAPI mime
// types. Each mime type is separated by a comma. Specify * to enable
// the policy for all mime types.
const char kEnableWin32kLockDownMimeTypes[] =
    "enable-win32k-lockdown-mimetypes";

// Enables experimental hardware acceleration for VP8/VP9 video decoding.
const char kEnableAcceleratedVpxDecode[] = "enable-accelerated-vpx-decode";

// DirectWrite FontCache is shared by browser to renderers using shared memory.
// This switch allows us to pass the shared memory handle to the renderer.
const char kFontCacheSharedHandle[] = "font-cache-shared-handle";

// Sets the free memory thresholds below which the system is considered to be
// under moderate and critical memory pressure. Used in the browser process,
// and ignored if invalid. Specified as a pair of comma separated integers.
// See base/win/memory_pressure_monitor.cc for defaults.
const char kMemoryPressureThresholdsMb[] = "memory-pressure-thresholds-mb";

// Enables the exporting of the tracing events to ETW. This is only supported on
// Windows Vista and later.
const char kTraceExportEventsToETW[] = "trace-export-events-to-etw";
#endif

// Don't dump stuff here, follow the same order as the header.

}  // namespace switches
