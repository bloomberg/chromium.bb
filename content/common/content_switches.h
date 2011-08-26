// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_H_
#pragma once

#include "build/build_config.h"

namespace switches {

extern const char kAllowFileAccessFromFiles[];
extern const char kAllowRunningInsecureContent[];
extern const char kAllowSandboxDebugging[];
extern const char kBrowserSubprocessPath[];
// TODO(jam): this doesn't belong in content.
extern const char kChromeFrame[];
extern const char kDisable3DAPIs[];
extern const char kDisableAccelerated2dCanvas[];
extern const char kDisableAcceleratedCompositing[];
extern const char kDisableAcceleratedLayers[];
extern const char kDisableAcceleratedVideo[];
extern const char kDisableAltWinstation[];
extern const char kDisableApplicationCache[];
extern const char kDisableAudio[];
extern const char kDisableBackingStoreLimit[];
extern const char kDisableDatabases[];
extern const char kDisableDataTransferItems[];
extern const char kDisableDesktopNotifications[];
extern const char kDisableDeviceOrientation[];
extern const char kDisableExperimentalWebGL[];
extern const char kDisableFileSystem[];
extern const char kDisableGeolocation[];
extern const char kDisableGLMultisampling[];
extern const char kDisableGLSLTranslator[];
extern const char kDisableGpuSandbox[];
extern const char kDisableGpuWatchdog[];
extern const char kDisableHangMonitor[];
extern const char kDisableIndexedDatabase[];
extern const char kDisableJava[];
extern const char kDisableJavaScript[];
extern const char kDisableJavaScriptI18NAPI[];
extern const char kDisableLocalStorage[];
extern const char kDisableLogging[];
extern const char kDisablePlugins[];
extern const char kDisablePopupBlocking[];
extern const char kDisableRendererAccessibility[];
extern const char kDisableSeccompSandbox[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
extern const char kDisableSpeechInput[];
extern const char kDisableSpellcheckAPI[];
extern const char kDisableWebAudio[];
extern const char kDisableWebSockets[];
extern const char kEnableAccelerated2dCanvas[];
extern const char kEnableAcceleratedDrawing[];
extern const char kEnableAcceleratedPlugins[];
extern const char kEnableAccessibility[];
extern const char kEnableBenchmarking[];
extern const char kEnableDeviceMotion[];
extern const char kEnableFullScreen[];
extern const char kEnableGPUPlugin[];
extern const char kEnableLogging[];
extern const char kEnableMediaStream[];
extern const char kEnableMonitorProfile[];
extern const char kEnablePreparsedJsCaching[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
extern const char kEnableStatsTable[];
extern const char kEnableVideoFullscreen[];
extern const char kEnableVideoLogging[];
extern const char kExperimentalLocationFeatures[];
// TODO(jam): this doesn't belong in content.
extern const char kExtensionProcess[];
extern const char kExtraPluginDir[];
extern const char kForceFieldTestNameAndValue[];
extern const char kForceRendererAccessibility[];
extern const char kGpuLauncher[];
extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
extern const char kIgnoreGpuBlacklist[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
extern const char kInProcessWebGL[];
extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];
extern const char kLowLatencyAudio[];
// TODO(jam): this doesn't belong in content.
extern const char kNaClBrokerProcess[];
extern const char kNaClLoaderProcess[];
// TODO(bradchen): remove kNaClLinuxHelper switch.
// This switch enables the experimental lightweight nacl_helper for Linux.
// It will be going away soon, when the helper is enabled permanently.
extern const char kNaClLinuxHelper[];
extern const char kNoDisplayingInsecureContent[];
extern const char kNoJsRandomness[];
extern const char kNoReferrers[];
extern const char kNoSandbox[];
extern const char kPlaybackMode[];
extern const char kPluginLauncher[];
extern const char kPluginPath[];
extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
extern const char kPpapiBrokerProcess[];
extern const char kPpapiFlashArgs[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kProcessPerSite[];
extern const char kProcessPerTab[];
extern const char kProcessType[];
// TODO(jam): this doesn't belong in content.
extern const char kProfileImportProcess[];
extern const char kRecordMode[];
extern const char kRegisterPepperPlugins[];
extern const char kRemoteShellPort[];
extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
extern const char kRendererCrashTest[];
extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
// TODO(jam): this doesn't belong in content.
extern const char kServiceProcess[];
extern const char kShowPaintRects[];
extern const char kSimpleDataSource[];
extern const char kSingleProcess[];
extern const char kSQLiteIndexedDatabase[];
extern const char kTestSandbox[];
extern const char kUnlimitedQuotaForFiles[];
extern const char kUnlimitedQuotaForIndexedDB[];
extern const char kUserAgent[];
extern const char kUtilityCmdPrefix[];
extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
extern const char kWorkerProcess[];
extern const char kZygoteCmdPrefix[];
extern const char kZygoteProcess[];

#if defined(OS_WIN)
extern const char kAuditHandles[];
extern const char kAuditAllHandles[];
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
extern const char kScrollPixels[];
#endif

#if !defined(OFFICIAL_BUILD)
extern const char kRendererCheckFalseTest[];
#endif

}  // namespace switches

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_H_
