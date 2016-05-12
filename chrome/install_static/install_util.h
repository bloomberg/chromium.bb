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

namespace install_static {

enum class ProcessType {
  UNINITIALIZED,
  NON_BROWSER_PROCESS,
  BROWSER_PROCESS,
};

// TODO(ananta)
// http://crbug.com/604923
// The constants defined in this file are also defined in chrome/installer and
// other places. we need to unify them.

extern const wchar_t kChromeChannelUnknown[];
extern const wchar_t kChromeChannelCanary[];
extern const wchar_t kChromeChannelDev[];
extern const wchar_t kChromeChannelBeta[];
extern const wchar_t kChromeChannelStable[];
extern const wchar_t kChromeChannelStableExplicit[];
extern const wchar_t kRegPathClientState[];
extern const wchar_t kRegPathClientStateMedium[];
extern const wchar_t kRegPathChromePolicy[];
extern const wchar_t kRegApField[];
extern const wchar_t kRegValueUsageStats[];
extern const wchar_t kUninstallArgumentsField[];
extern const wchar_t kMetricsReportingEnabled[];
extern const wchar_t kAppGuidCanary[];
extern const wchar_t kAppGuidGoogleChrome[];
extern const wchar_t kAppGuidGoogleBinaries[];

// Returns true if |exe_path| points to a Chrome installed in an SxS
// installation.
bool IsSxSChrome(const wchar_t* exe_path);

// Returns true if |exe_path| points to a per-user level Chrome installation.
bool IsSystemInstall(const wchar_t* exe_path);

// Returns true if current installation of Chrome is a multi-install.
bool IsMultiInstall(bool is_system_install);

// Returns true if usage stats collecting is enabled for this user for the
// current executable.
bool GetCollectStatsConsent();

// Returns true if usage stats collecting is enabled for this user for the
// executable passed in as |exe_path|.
// Only used by tests.
bool GetCollectStatsConsentForTesting(const base::string16& exe_path);

// Returns true if if usage stats reporting is controlled by a mandatory
// policy. |metrics_is_enforced_by_policy| will be set to true accordingly.
// TODO(ananta)
// Make this function private to install_util.
bool ReportingIsEnforcedByPolicy(bool* metrics_is_enforced_by_policy);

// Initializes |g_process_type| which stores whether or not the current
// process is the main browser process.
void InitializeProcessType();

// Returns true if invoked in a Chrome process other than the main browser
// process. False otherwise.
bool IsNonBrowserProcess();

// Populates |result| with the default User Data directory for the current
// user.This may be overidden by a command line option.Returns false if all
// attempts at locating a User Data directory fail
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetDefaultUserDataDirectory(base::string16* result);

// Populates |crash_dir| with the default crash dump location regardless of
// whether DIR_USER_DATA or DIR_CRASH_DUMPS has been overridden.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetDefaultCrashDumpLocation(base::string16* crash_dir);

// Returns the contents of the specified |variable_name| from the environment
// block of the calling process. Returns an empty string if the variable does
// not exist.
std::string GetEnvironmentString(const std::string& variable_name);

// Sets the environment variable identified by |variable_name| to the value
// identified by |new_value|.
bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value);

// Returns true if the environment variable identified by |variable_name|
// exists.
bool HasEnvironmentVariable(const std::string& variable_name);

// Gets the exe version details like the |product_name|, |version|,
// |special_build|, |channel_name|, etc. Most of this information is read
// from the version resource. |exe_path| is the path of chrome.exe.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetExecutableVersionDetails(const base::string16& exe_path,
                                  base::string16* product_name,
                                  base::string16* version,
                                  base::string16* special_build,
                                  base::string16* channel_name);

// Gets the channel name for the current Chrome process.
// If |add_modifier| is true the channel name is returned with the modifier
// prepended to it. Currently this is only done for multi installs, i.e (-m)
// is the only modifier supported.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
void GetChromeChannelName(bool is_per_user_install,
                          bool add_modifier,
                          base::string16* channel_name);


// Returns the version of Google Update that is installed.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
std::string GetGoogleUpdateVersion();

// Returns the Chrome installation subdirectory, i.e. Google Chrome\Chromium,
// etc.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
base::string16 GetChromeInstallSubDirectory();

// Returns the registry path where the browser crash dumps metrics need to be
// written to.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the version in
// chrome\common\metrics_constants_util_win.cc
base::string16 GetBrowserCrashDumpAttemptsRegistryPath();

// Returns true if the |source| string matches the |pattern|. The pattern
// may contain wildcards like '?', which matches one character or a '*'
// which matches 0 or more characters.
// Please note that pattern matches the whole string. If you want to find
// something in the middle of the string then you need to specify the pattern
// as '*xyz*'.
bool MatchPattern(const base::string16& source, const base::string16& pattern);

// Caches the |ProcessType| of the current process.
extern ProcessType g_process_type;

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
