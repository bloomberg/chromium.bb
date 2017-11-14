// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_

#include <limits.h>
#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/time/time.h"

namespace base {
class FilePath;
class TaskRunner;
class Version;
}

namespace safe_browsing {

// A special exit code identifying a failure to run the reporter.
const int kReporterNotLaunchedExitCode = INT_MAX;

// The number of days to wait before triggering another reporter run.
const int kDaysBetweenSuccessfulSwReporterRuns = 7;
// The number of days to wait before sending out reporter logs.
const int kDaysBetweenReporterLogsSent = 7;

// Parameters used to invoke the sw_reporter component.
struct SwReporterInvocation {
  base::CommandLine command_line;

  // Experimental versions of the reporter will write metrics to registry keys
  // ending in |suffix|. Those metrics should be copied to UMA histograms also
  // ending in |suffix|. For the canonical version, |suffix| will be empty.
  std::string suffix;

  // Flags to control behaviours the Software Reporter should support by
  // default. These flags are set in the Reporter installer, and experimental
  // versions of the reporter will turn on the behaviours that are not yet
  // supported.
  using Behaviours = uint32_t;
  enum : Behaviours {
    BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS = 0x2,
    BEHAVIOUR_TRIGGER_PROMPT = 0x4,
    BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS = 0x8,
  };
  Behaviours supported_behaviours = 0;

  // Whether logs upload was enabled in this invocation.
  bool logs_upload_enabled = false;

  SwReporterInvocation();

  static SwReporterInvocation FromFilePath(const base::FilePath& exe_path);
  static SwReporterInvocation FromCommandLine(
      const base::CommandLine& command_line);

  bool operator==(const SwReporterInvocation& other) const;

  bool BehaviourIsSupported(Behaviours intended_behaviour) const;
};

using SwReporterQueue = std::queue<SwReporterInvocation>;

// Tries to run the sw_reporter component. If this runs successfully, than any
// calls made in the next |kDaysBetweenSuccessfulSwReporterRuns| days will be
// ignored.
//
// Each "run" of the sw_reporter component may aggregate the results of several
// executions of the tool with different command lines. |invocations| is the
// queue of SwReporters to execute as a single "run". When a new try is
// scheduled the entire queue is executed.
//
// |version| is the version of the tool that will run.
//
// When finished, will call |on_sequence_done|.
void RunSwReportersWithCallback(const SwReporterQueue& invocations,
                                const base::Version& version,
                                base::OnceClosure on_sequence_done);

// Same as RunSwReportersWithCallback, with a default no-op callback.
void RunSwReporters(const SwReporterQueue& invocations,
                    const base::Version& version);

// Returns true iff Local State is successfully accessed and indicates the most
// recent Reporter run terminated with an exit code indicating the presence of
// UwS.
bool ReporterFoundUws();

// Returns true iff a valid registry key for the SRT Cleaner exists, and that
// key is nonempty.
// TODO(tmartino): Consider changing to check whether the user has recently
// run the cleaner, rather than checking if they've run it at all.
bool UserHasRunCleaner();

// A delegate used by tests to implement test doubles (e.g., stubs, fakes, or
// mocks).
class SwReporterTestingDelegate {
 public:
  virtual ~SwReporterTestingDelegate() {}

  // Invoked by tests in place of base::LaunchProcess.
  virtual int LaunchReporter(const SwReporterInvocation& invocation) = 0;

  // Invoked by tests in place of the actual prompting logic.
  // See MaybeFetchSRT().
  virtual void TriggerPrompt() = 0;

  // Invoked by tests to override the current time.
  // See Now() in reporter_runner_win.cc.
  virtual base::Time Now() const = 0;

  // A task runner used to spawn the reporter process (which blocks).
  // See ReporterRunner::ScheduleNextInvocation().
  virtual base::TaskRunner* BlockingTaskRunner() const = 0;
};

// Set a delegate for testing. The implementation will not take ownership of
// |delegate| - it must remain valid until this function is called again to
// reset the delegate. If |delegate| is nullptr, any previous delegate is
// cleared.
void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_
