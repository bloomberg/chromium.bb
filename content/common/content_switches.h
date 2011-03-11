// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_H_
#pragma once

namespace switches {

extern const char kAllowFileAccessFromFiles[];
extern const char kAllowSandboxDebugging[];
extern const char kBrowserSubprocessPath[];
extern const char kDisableBackingStoreLimit[];
extern const char kDisableFileSystem[];
extern const char kDisableGpuSandbox[];
extern const char kDisableGpuWatchdog[];
extern const char kDisableHolePunching[];
extern const char kDisableLogging[];
extern const char kDisablePlugins[];
extern const char kDisablePopupBlocking[];
extern const char kDisableSeccompSandbox[];
extern const char kDisableWebSockets[];
extern const char kEnableBenchmarking[];
extern const char kEnableGPUPlugin[];
extern const char kEnableLogging[];
extern const char kEnableMonitorProfile[];
extern const char kEnablePreparsedJsCaching[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
extern const char kEnableStatsTable[];
extern const char kExperimentalLocationFeatures[];
extern const char kExtraPluginDir[];
extern const char kGpuLauncher[];
extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
extern const char kLevelDBIndexedDatabase[];
extern const char kLoadPlugin[];
extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];
extern const char kNoGpuSandbox[];
extern const char kNoReferrers[];
extern const char kPluginLauncher[];
extern const char kPluginPath[];
extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
extern const char kPpapiFlashPath[];
extern const char kPpapiFlashVersion[];
extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kProcessPerSite[];
extern const char kProcessPerTab[];
extern const char kProcessType[];
extern const char kRegisterPepperPlugins[];
extern const char kRendererCmdPrefix[];
extern const char kRendererCrashTest[];
extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
extern const char kSafePlugins[];
extern const char kSingleProcess[];
extern const char kTestSandbox[];
extern const char kUnlimitedQuotaForFiles[];
extern const char kUnlimitedQuotaForIndexedDB[];
extern const char kUserAgent[];
extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
extern const char kWorkerProcess[];
extern const char kZygoteCmdPrefix[];
extern const char kZygoteProcess[];

}  // namespace switches

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_H_
