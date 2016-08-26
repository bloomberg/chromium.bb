// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_

#include <limits.h>

#include <string>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"

namespace base {
class FilePath;
class TaskRunner;
}

class Browser;

namespace safe_browsing {

// SRT registry keys and value names.
extern const wchar_t kSoftwareRemovalToolRegistryKey[];
extern const wchar_t kEndTimeValueName[];
extern const wchar_t kStartTimeValueName[];

// Reporter exit codes.
const int kSwReporterCleanupNeeded = 0;
const int kSwReporterNothingFound = 2;
const int kSwReporterPostRebootCleanupNeeded = 4;
const int kSwReporterDelayedPostRebootCleanupNeeded = 15;

// A special exit code identifying a failure to run the reporter.
const int kReporterFailureExitCode = INT_MAX;

// The number of days to wait before triggering another reporter run.
const int kDaysBetweenSuccessfulSwReporterRuns = 7;
const int kDaysBetweenSwReporterRunsForPendingPrompt = 1;

// Parameters used to invoke the sw_reporter component.
struct SwReporterInvocation {
  base::CommandLine command_line;

  // Experimental versions of the reporter will write metrics to registry keys
  // ending in |suffix|. Those metrics should be copied to UMA histograms also
  // ending in |suffix|. For the canonical version, |suffix| will be empty.
  std::string suffix;

  // The experimental sw_reporter never triggers the prompt, just reports
  // results through UMA.
  bool is_experimental = false;

  SwReporterInvocation();

  static SwReporterInvocation FromFilePath(const base::FilePath& exe_path);
  static SwReporterInvocation FromCommandLine(
      const base::CommandLine& command_line);

  bool operator==(const SwReporterInvocation& other) const;
};

// Tries to run the sw_reporter component, and then schedule the next try. If
// called multiple times, then multiple sequences of trying to run will happen,
// yet only one reporter will run per specified period (either
// |kDaysBetweenSuccessfulSwReporterRuns| or
// |kDaysBetweenSwReporterRunsForPendingPrompt|) will actually happen.
// |invocation| is the details of the SwReporter to execute, and |version| is
// its version. The task runners are provided to allow tests to provide their
// own.
void RunSwReporter(const SwReporterInvocation& invocation,
                   const base::Version& version,
                   scoped_refptr<base::TaskRunner> main_thread_task_runner,
                   scoped_refptr<base::TaskRunner> blocking_task_runner);

// Returns true iff Local State is successfully accessed and indicates the most
// recent Reporter run terminated with an exit code indicating the presence of
// UwS.
bool ReporterFoundUws();

// Returns true iff a valid registry key for the SRT Cleaner exists, and that
// key is nonempty.
// TODO(tmartino): Consider changing to check whether the user has recently
// run the cleaner, rather than checking if they've run it at all.
bool UserHasRunCleaner();

// Mocks and callbacks for the unit tests.
class SwReporterTestingDelegate {
 public:
  virtual ~SwReporterTestingDelegate() {}

  // Test mock for launching the reporter.
  virtual int LaunchReporter(const SwReporterInvocation& invocation) = 0;

  // Test mock for showing the prompt.
  virtual void TriggerPrompt(Browser* browser,
                             const std::string& reporter_version) = 0;

  // Callback to let the tests know the reporter is ready to launch.
  virtual void NotifyLaunchReady() = 0;

  // Callback to let the tests know the reporter has finished running.
  virtual void NotifyReporterDone() = 0;
};

// Set a delegate for testing. The implementation will not take ownership of
// |delegate| - it must remain valid until this function is called again to
// reset the delegate. If |delegate| is nullptr, any previous delegate is
// cleared.
void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FETCHER_WIN_H_
