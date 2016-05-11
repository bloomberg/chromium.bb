// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This file contains helper functions which provide information about the
// current version of Chrome. This includes channel information, version
// information etc. This functionality is provided by using functions in
// kernel32 and advapi32. No other dependencies are allowed in this file.

#ifndef CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
#define CHROME_INSTALL_STATIC_INSTALL_UTIL_H_

#include "base/strings/string16.h"

enum class ProcessType {
  UNINITIALIZED,
  NON_BROWSER_PROCESS,
  BROWSER_PROCESS,
};

// Returns true if |exe_path| points to a Chrome installed in an SxS
// installation.
bool IsCanary(const wchar_t* exe_path);

// Returns true if |exe_path| points to a per-user level Chrome installation.
bool IsSystemInstall(const wchar_t* exe_path);

// Returns true if current installation of Chrome is a multi-install.
bool IsMultiInstall(bool is_system_install);

// Returns true if usage stats collecting is enabled for this user.
bool AreUsageStatsEnabled(const wchar_t* exe_path);

// Returns true if a policy is in effect. |breakpad_enabled| will be set to true
// if stats collecting is permitted by this policy and false if not.
bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);

// Initializes |g_process_type| which stores whether or not the current process
// is the main browser process.
void InitializeProcessType();

// Returns true if invoked in a Chrome process other than the main browser
// process. False otherwise.
bool IsNonBrowserProcess();

// Caches the |ProcessType| of the current process.
extern ProcessType g_process_type;

#endif  // CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
