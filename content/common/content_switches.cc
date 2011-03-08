// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches.h"

namespace switches {

// By default, file:// URIs cannot read other file:// URIs. This is an
// override for developers who need the old behavior for testing.
const char kAllowFileAccessFromFiles[]      = "allow-file-access-from-files";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[]         = "allow-sandbox-debugging";

// Disable limits on the number of backing stores. Can prevent blinking for
// users with many windows/tabs and lots of memory.
const char kDisableBackingStoreLimit[]      = "disable-backing-store-limit";

// Disable FileSystem API.
const char kDisableFileSystem[]             = "disable-file-system";

// Temporary flag to disable hole punching for accelerated surfaces. This is
// here to aid debugging eventual problems, it can be removed once hole punching
// has been out there for a few dev channel releases without problems.
const char kDisableHolePunching[]           = "disable-hole-punching";

// Force logging to be disabled.  Logging is enabled by default in debug
// builds.
const char kDisableLogging[]                = "disable-logging";

// Disable the GPU process sandbox.
const char kDisableGpuSandbox[]             = "disable-gpu-sandbox";

// Disable the thread that crashes the GPU process if it stops responding to
// messages.
const char kDisableGpuWatchdog[]            = "disable-gpu-watchdog";

// Prevent plugins from running.
const char kDisablePlugins[]                = "disable-plugins";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Disable the seccomp sandbox (Linux only)
const char kDisableSeccompSandbox[]         = "disable-seccomp-sandbox";

// Disable Web Sockets support.
const char kDisableWebSockets[]             = "disable-web-sockets";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enable the GPU plugin and Pepper 3D rendering.
const char kEnableGPUPlugin[]               = "enable-gpu-plugin";

// Force logging to be enabled.  Logging is disabled by default in release
// builds.
const char kEnableLogging[]                 = "enable-logging";

// On Windows, converts the page to the currently-installed monitor profile.
// This does NOT enable color management for images. The source is still
// assumed to be sRGB.
const char kEnableMonitorProfile[]          = "enable-monitor-profile";

// Enable caching of pre-parsed JS script data.  See http://crbug.com/32407.
const char kEnablePreparsedJsCaching[]      = "enable-preparsed-js-caching";

// Cause the OS X sandbox write to syslog every time an access to a resource
// is denied by the sandbox.
const char kEnableSandboxLogging[]          = "enable-sandbox-logging";

// Enable the seccomp sandbox (Linux only)
const char kEnableSeccompSandbox[]          = "enable-seccomp-sandbox";

// Enables StatsTable, logging statistics to a global named shared memory table.
const char kEnableStatsTable[]              = "enable-stats-table";

// Enables experimental features for the geolocation API.
// Current features:
// - CoreLocation support for Mac OS X 10.6
// - Gateway location for Linux and Windows
// - Location platform support for Windows 7
const char kExperimentalLocationFeatures[]  = "experimental-location-features";

// Load NPAPI plugins from the specified directory.
const char kExtraPluginDir[]                = "extra-plugin-dir";

// Extra command line options for launching the GPU process (normally used
// for debugging). Use like renderer-cmd-prefix.
const char kGpuLauncher[]                   = "gpu-launcher";

// Makes this process a GPU sub-process.
const char kGpuProcess[]                    = "gpu-process";

// Causes the GPU process to display a dialog on launch.
const char kGpuStartupDialog[]              = "gpu-startup-dialog";

// Load an NPAPI plugin from the specified path.
const char kLoadPlugin[]                    = "load-plugin";

// Sets the minimum log level. Valid values are from 0 to 3:
// INFO = 0, WARNING = 1, LOG_ERROR = 2, LOG_FATAL = 3.
const char kLoggingLevel[]                  = "log-level";

// Make plugin processes log their sent and received messages to VLOG(1).
const char kLogPluginMessages[]             = "log-plugin-messages";

// Don't Sandbox the GPU process, does not affect other sandboxed processes.
const char kNoGpuSandbox[]                  = "no-gpu-sandbox";

// Don't send HTTP-Referer headers.
const char kNoReferrers[]                   = "no-referrers";

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

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const char kProcessType[]                   = "type";

// Register Pepper plugins (see pepper_plugin_registry.cc for its format).
const char kRegisterPepperPlugins[]         = "register-pepper-plugins";

// On POSIX only: the contents of this flag are prepended to the renderer
// command line. Useful values might be "valgrind" or "xterm -e gdb --args".
const char kRendererCmdPrefix[]             = "renderer-cmd-prefix";

// Causes the renderer process to crash on launch.
const char kRendererCrashTest[]             = "renderer-crash-test";

// Causes the process to run as renderer instead of as browser.
const char kRendererProcess[]               = "renderer";

// Causes the renderer process to display a dialog on launch.
const char kRendererStartupDialog[]         = "renderer-startup-dialog";

// Runs the plugin processes inside the sandbox.
const char kSafePlugins[]                   = "safe-plugins";

// Runs the renderer and plugins in the same process as the browser
const char kSingleProcess[]                 = "single-process";

// Runs the security test for the renderer sandbox.
const char kTestSandbox[]                   = "test-sandbox";

// Grant unlimited quota to store files to this process.
// Used for testing Pepper's FileRef/FileIO/FileSystem implementations.
// DO NOT USE FOR OTHER PURPOSES.
// TODO(dumi): remove the switch when we have a real quota implementation.
const char kUnlimitedQuotaForFiles[]        = "unlimited-quota-for-files";

// This is for testing IndexedDB and will give any website you visit unlimited
// quota in IndexedDB. This should only be used for development and not general
// browsing. It is ignored in single process mode.
const char kUnlimitedQuotaForIndexedDB[]    = "unlimited-quota-for-indexeddb";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

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
}  // namespace switches
