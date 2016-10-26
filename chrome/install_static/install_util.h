// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This file contains helper functions which provide information about the
// current version of Chrome. This includes channel information, version
// information etc. This functionality is provided by using functions in
// kernel32 and advapi32. No other dependencies are allowed in this file.

#ifndef CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
#define CHROME_INSTALL_STATIC_INSTALL_UTIL_H_

#include <string>
#include <vector>

namespace install_static {

enum class ProcessType {
  UNINITIALIZED,
  NON_BROWSER_PROCESS,
  BROWSER_PROCESS,
};

// TODO(ananta)
// https://crbug.com/604923
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
// Registry key to store the stats/crash sampling state of Chrome. If set to 1,
// stats and crash reports will be uploaded in line with the user's consent,
// otherwise, uploads will be disabled. It is used to sample clients, to reduce
// server load for metics and crashes. This is controlled by the
// MetricsReporting feature in chrome_metrics_services_manager_client.cc and is
// written when metrics services are started up and when consent changes.
extern const wchar_t kRegValueChromeStatsSample[];
// Used to retrieve consent for uploading crashes and metrics.
extern const wchar_t kRegValueUsageStats[];
extern const wchar_t kUninstallArgumentsField[];
extern const wchar_t kMetricsReportingEnabled[];
extern const wchar_t kAppGuidCanary[];
extern const wchar_t kAppGuidGoogleChrome[];
extern const wchar_t kAppGuidGoogleBinaries[];

// TODO(ananta)
// https://crbug.com/604923
// Unify these constants with env_vars.h.
extern const wchar_t kHeadless[];
extern const wchar_t kShowRestart[];
extern const wchar_t kRestartInfo[];
extern const wchar_t kRtlLocale[];

// TODO(ananta)
// https://crbug.com/604923
// Unify these constants with those defined in content_switches.h.
extern const char kGpuProcess[];
extern const char kPpapiPluginProcess[];
extern const char kRendererProcess[];
extern const char kUtilityProcess[];
extern const char kProcessType[];
extern const char kCrashpadHandler[];

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
bool GetCollectStatsConsentForTesting(const std::wstring& exe_path);

// Returns true if the current executable is currently in the chosen sample that
// will report stats and crashes.
bool GetCollectStatsInSample();

// Sets the registry value used for checking if Chrome is in the chosen sample
// that will report stats and crashes. Returns true if writing was successful.
bool SetCollectStatsInSample(bool in_sample);

// Returns true if if usage stats reporting is controlled by a mandatory
// policy. |crash_reporting_enabled| determines whether it's enabled (true) or
// disabled (false).
bool ReportingIsEnforcedByPolicy(bool* crash_reporting_enabled);

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
bool GetDefaultUserDataDirectory(std::wstring* result);

// Populates |crash_dir| with the default crash dump location regardless of
// whether DIR_USER_DATA or DIR_CRASH_DUMPS has been overridden.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetDefaultCrashDumpLocation(std::wstring* crash_dir);

// Returns the contents of the specified |variable_name| from the environment
// block of the calling process. Returns an empty string if the variable does
// not exist.
std::string GetEnvironmentString(const std::string& variable_name);
std::wstring GetEnvironmentString16(const std::wstring& variable_name);

// Sets the environment variable identified by |variable_name| to the value
// identified by |new_value|.
bool SetEnvironmentString(const std::string& variable_name,
                          const std::string& new_value);
bool SetEnvironmentString16(const std::wstring& variable_name,
                            const std::wstring& new_value);

// Returns true if the environment variable identified by |variable_name|
// exists.
bool HasEnvironmentVariable(const std::string& variable_name);
bool HasEnvironmentVariable16(const std::wstring& variable_name);

// Gets the exe version details like the |product_name|, |version|,
// |special_build|, |channel_name|, etc. Most of this information is read
// from the version resource. |exe_path| is the path of chrome.exe.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
bool GetExecutableVersionDetails(const std::wstring& exe_path,
                                 std::wstring* product_name,
                                 std::wstring* version,
                                 std::wstring* special_build,
                                 std::wstring* channel_name);

// Gets the channel name for the current Chrome process.
// If |add_modifier| is true the channel name is returned with the modifier
// prepended to it. Currently this is only done for multi installs, i.e (-m)
// is the only modifier supported.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
void GetChromeChannelName(bool is_per_user_install,
                          bool add_modifier,
                          std::wstring* channel_name);

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
std::wstring GetChromeInstallSubDirectory();

// Returns the registry path where the browser crash dumps metrics need to be
// written to.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the version in
// chrome\common\metrics_constants_util_win.cc
std::wstring GetBrowserCrashDumpAttemptsRegistryPath();

// Returns true if the |source| string matches the |pattern|. The pattern
// may contain wildcards like '?', which matches one character or a '*'
// which matches 0 or more characters.
// Please note that pattern matches the whole string. If you want to find
// something in the middle of the string then you need to specify the pattern
// as '*xyz*'.
bool MatchPattern(const std::wstring& source, const std::wstring& pattern);

// UTF8 to UTF16 and vice versa conversion helpers.
std::wstring UTF8ToUTF16(const std::string& source);

std::string UTF16ToUTF8(const std::wstring& source);

// Tokenizes a string |str| based on single character delimiter.
// The tokens are returned in a vector. The |trim_spaces| parameter indicates
// whether the function should optionally trim spaces throughout the string.
std::vector<std::string> TokenizeString(const std::string& str,
                                        char delimiter,
                                        bool trim_spaces);
std::vector<std::wstring> TokenizeString16(const std::wstring& str,
                                           wchar_t delimiter,
                                           bool trim_spaces);

// Compares version strings of the form "X.X.X.X" and returns the result of the
// comparison in the |result| parameter. The result is as below:
// 0 if the versions are equal.
// -1 if version1 < version2.
// 1 if version1 > version2.
// Returns true on success, false on invalid strings being passed, etc.
bool CompareVersionStrings(const std::string& version1,
                           const std::string& version2,
                           int* result);

// We assume that the command line |command_line| contains multiple switches
// with the format --<switch name>=<switch value>. This function returns the
// value of the |switch_name| passed in.
std::string GetSwitchValueFromCommandLine(const std::string& command_line,
                                          const std::string& switch_name);

// Caches the |ProcessType| of the current process.
extern ProcessType g_process_type;

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
