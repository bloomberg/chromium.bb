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

struct InstallConstants;

enum class ProcessType {
  UNINITIALIZED,
  NON_BROWSER_PROCESS,
  BROWSER_PROCESS,
};

// Registry key to store the stats/crash sampling state of Chrome. If set to 1,
// stats and crash reports will be uploaded in line with the user's consent,
// otherwise, uploads will be disabled. It is used to sample clients, to reduce
// server load for metics and crashes. This is controlled by the
// MetricsReporting feature in chrome_metrics_services_manager_client.cc and is
// written when metrics services are started up and when consent changes.
extern const wchar_t kRegValueChromeStatsSample[];

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
extern const wchar_t kCrashpadHandler[];
extern const wchar_t kProcessType[];
extern const wchar_t kUserDataDirSwitch[];
extern const wchar_t kUtilityProcess[];

// Used for suppressing warnings.
template <typename T> inline void IgnoreUnused(T) {}

// Returns true if Chrome is running at system level.
bool IsSystemInstall();

// Returns true if current installation of Chrome is a multi-install.
bool IsMultiInstall();

// Returns true if usage stats collecting is enabled for this user for the
// current executable.
bool GetCollectStatsConsent();

// Returns true if the current executable is currently in the chosen sample that
// will report stats and crashes.
bool GetCollectStatsInSample();

// Sets the registry value used for checking if Chrome is in the chosen sample
// that will report stats and crashes. Returns true if writing was successful.
bool SetCollectStatsInSample(bool in_sample);

// Appends "[kCompanyPathName\]kProductPathName[install_suffix]" to |path|,
// returning a reference to |path|.
std::wstring& AppendChromeInstallSubDirectory(const InstallConstants& mode,
                                              bool include_suffix,
                                              std::wstring* path);

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

// Populates |crash_dir| with the crash dump location, respecting modifications
// to user-data-dir.
// TODO(ananta)
// http://crbug.com/604923
// Unify this with the Browser Distribution code.
std::wstring GetCrashDumpLocation();

// Returns the contents of the specified |variable_name| from the environment
// block of the calling process. Returns an empty string if the variable does
// not exist.
std::string GetEnvironmentString(const std::string& variable_name);
std::wstring GetEnvironmentString16(const wchar_t* variable_name);

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
void GetExecutableVersionDetails(const std::wstring& exe_path,
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
std::wstring GetChromeChannelName(bool add_modifier);

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

// We assume that the command line |command_line| contains multiple switches
// with the format --<switch name>=<switch value>. This function returns the
// value of the |switch_name| passed in.
std::wstring GetSwitchValueFromCommandLine(const std::wstring& command_line,
                                           const std::wstring& switch_name);

// Ensures that the given |full_path| exists, and that the tail component is a
// directory. If the directory does not already exist, it will be created.
// Returns false if the final component exists but is not a directory, or on
// failure to create a directory.
bool RecursiveDirectoryCreate(const std::wstring& full_path);

// Returns the unadorned channel name based on the channel strategy for the
// install mode.
std::wstring DetermineChannel(const InstallConstants& mode,
                              bool system_level,
                              bool multi_install);

// Caches the |ProcessType| of the current process.
extern ProcessType g_process_type;

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_UTIL_H_
