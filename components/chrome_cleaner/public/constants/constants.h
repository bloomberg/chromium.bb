// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_
#define COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_

// Constants shared by the Chromium and the Chrome Cleanaup tool repos.

namespace chrome_cleaner {

// Switches sent from Chrome to either the Software Reporter or the Chrome
// Cleanup tool.

// The current Chrome channel. The value of this flag is an integer with values
// according to version_info::Channel enum.
extern const char kChromeChannelSwitch[];

// The path to Chrome's executable.
extern const char kChromeExePathSwitch[];

// The Mojo pipe token for IPC communication between the Software Reporter and
// Chrome.
extern const char kChromeMojoPipeTokenSwitch[];

// Indicates that a cleaner run was started by Chrome.
extern const char kChromePromptSwitch[];

// Indicates that the current Chrome installation was a system-level
// installation.
extern const char kChromeSystemInstallSwitch[];

// The Chrome version string.
extern const char kChromeVersionSwitch[];

// Indicates that crash reporting is enabled for the current user.
extern const char kEnableCrashReportingSwitch[];

// Indicates that the current user opted into Safe Browsing Extended Reporting.
extern const char kExtendedSafeBrowsingEnabledSwitch[];

// Identifier used to group all reports generated during the same run of the
// software reporter (which may include multiple invocations of the reporter
/// binary, each generating a report). An ASCII, base-64 encoded random string.
extern const char kSessionIdSwitch[];

// Indicates that metrics reporting is enabled for the current user.
extern const char kUmaUserSwitch[];

// Registry paths where the reporter and the cleaner will write metrics data
// to be reported by Chrome.

// TODO(b/647763) Change the registry key to properly handle cases when the
// user runs Google Chrome stable alongside Google Chrome SxS.
extern const wchar_t kSoftwareRemovalToolRegistryKey[];

// The suffix for the registry key where cleaner metrics are written to.
extern const wchar_t kCleanerSubKey[];
// The suffix for registry key paths where scan times will be written to.
extern const wchar_t kScanTimesSubKey[];

// Registry value names where metrics are written to.
extern const wchar_t kEndTimeValueName[];
extern const wchar_t kEngineErrorCodeValueName[];
extern const wchar_t kExitCodeValueName[];
extern const wchar_t kFoundUwsValueName[];
extern const wchar_t kLogsUploadResultValueName[];
extern const wchar_t kMemoryUsedValueName[];
extern const wchar_t kStartTimeValueName[];
extern const wchar_t kUploadResultsValueName[];
extern const wchar_t kVersionValueName[];

// Exit codes from the Software Reporter process identified by Chrome.
constexpr int kSwReporterCleanupNeeded = 0;
constexpr int kSwReporterNothingFound = 2;
constexpr int kSwReporterPostRebootCleanupNeeded = 4;
constexpr int kSwReporterDelayedPostRebootCleanupNeeded = 15;

// Values to be passed to the kChromePromptSwitch of the Chrome Cleanup Tool to
// indicate how the user interacted with the accept button.
enum class ChromePromptValue {
  // The user accepted the prompt when the prompt was first shown.
  kPrompted = 3,
  // The user accepted the prompt after navigating to it from the menu.
  kShownFromMenu = 4
};

}  // namespace chrome_cleaner

#endif  // COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_CONSTANTS_H_
