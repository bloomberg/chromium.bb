// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_MAIN_H_
#define CHROME_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/tracked_objects.h"
#include "content/browser/browser_main.h"

class FieldTrialSynchronizer;
class HistogramSynchronizer;
class MetricsService;
class PrefService;
class ShutdownWatcherHelper;

namespace chrome_browser {
// For use by ShowMissingLocaleMessageBox.
extern const char kMissingLocaleDataTitle[];
extern const char kMissingLocaleDataMessage[];
}

class ChromeBrowserMainParts : public content::BrowserMainParts {
 public:
  virtual ~ChromeBrowserMainParts();

  // Constructs HistogramSynchronizer which gets released early (before
  // main_message_loop_).
  void SetupHistogramSynchronizer();

  // Constructs metrics service and does related initialization, including
  // creation of field trials. Call only after labs have been converted to
  // switches.
  MetricsService* SetupMetricsAndFieldTrials(
      const CommandLine& parsed_command_line,
      PrefService* local_state);

 protected:
  explicit ChromeBrowserMainParts(const MainFunctionParams& parameters);

  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual void ToolkitInitialized() OVERRIDE;

  virtual int TemporaryContinue() OVERRIDE;

 private:
  // Methods for |EarlyInitialization()| ---------------------------------------

  // A/B test for the maximum number of persistent connections per host.
  void ConnectionFieldTrial();

  // A/B test for determining a value for unused socket timeout.
  void SocketTimeoutFieldTrial();

  // A/B test for the maximum number of connections per proxy server.
  void ProxyConnectionsFieldTrial();

  // A/B test for spdy when --use-spdy not set.
  void SpdyFieldTrial();

  // A/B test for warmest socket vs. most recently used socket.
  void WarmConnectionFieldTrial();

  // A/B test for automatically establishing a backup TCP connection when a
  // specified timeout value is reached.
  void ConnectBackupJobsFieldTrial();

  // A/B test for using a different host prefix in Google search suggest.
  void SuggestPrefixFieldTrial();

  // Methods for |SetupMetricsAndFieldTrials()| --------------------------------

  static MetricsService* InitializeMetrics(
      const CommandLine& parsed_command_line,
      const PrefService* local_state);

  // Add an invocation of your field trial init function to this method.
  void SetupFieldTrials(bool metrics_recording_enabled,
                        bool proxy_policy_is_set);

  // Members initialized on construction ---------------------------------------

  // Create ShutdownWatcherHelper object for watching jank during shutdown.
  // Please keep |shutdown_watcher| as the first object constructed, and hence
  // it is destroyed last.
  scoped_ptr<ShutdownWatcherHelper> shutdown_watcher_;

#if defined(TRACK_ALL_TASK_OBJECTS)
  // Creating this object starts tracking the creation and deletion of Task
  // instance. This MUST be done before main_message_loop, so that it is
  // destroyed after the main_message_loop.
  tracked_objects::AutoTracking tracking_objects_;
#endif

  // Statistical testing infrastructure for the entire browser. NULL until
  // SetupMetricsAndFieldTrials is called.
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  // Members initialized after / released before main_message_loop_ ------------

  // Initialized in SetupHistogramSynchronizer.
  scoped_refptr<HistogramSynchronizer> histogram_synchronizer_;

  // Initialized in SetupMetricsAndFieldTrials.
  scoped_refptr<FieldTrialSynchronizer> field_trial_synchronizer_;

  FRIEND_TEST(BrowserMainTest, WarmConnectionFieldTrial_WarmestSocket);
  FRIEND_TEST(BrowserMainTest, WarmConnectionFieldTrial_Random);
  FRIEND_TEST(BrowserMainTest, WarmConnectionFieldTrial_Invalid);
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainParts);
};

// Records the conditions that can prevent Breakpad from generating and
// sending crash reports.  The presence of a Breakpad handler (after
// attempting to initialize crash reporting) and the presence of a debugger
// are registered with the UMA metrics service.
void RecordBreakpadStatusUMA(MetricsService* metrics);

// Displays a warning message if some minimum level of OS support is not
// present on the current platform.
void WarnAboutMinimumSystemRequirements();

// Displays a warning message that we can't find any locale data files.
void ShowMissingLocaleMessageBox();

// Records the time from our process' startup to the present time in
// the UMA histogram |metric_name|.
void RecordBrowserStartupTime();

// Records a time value to an UMA histogram in the context of the
// PreReadExperiment field-trial. This also reports to the appropriate
// sub-histogram (_PreRead(Enabled|Disabled)).
void RecordPreReadExperimentTime(const char* name, base::TimeDelta time);

#endif  // CHROME_BROWSER_BROWSER_MAIN_H_
