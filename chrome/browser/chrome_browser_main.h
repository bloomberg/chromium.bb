// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/field_trial.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/task_profiler/auto_tracking.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "content/public/browser/browser_main_parts.h"

class BrowserProcessImpl;
class ChromeBrowserMainExtraParts;
class FieldTrialSynchronizer;
class MetricsService;
class PrefService;
class Profile;
class StartupBrowserCreator;
class StartupTimeBomb;
class ShutdownWatcherHelper;
class ThreeDAPIObserver;
class TranslateManager;

namespace chrome_browser {
// For use by ShowMissingLocaleMessageBox.
extern const char kMissingLocaleDataTitle[];
extern const char kMissingLocaleDataMessage[];
}

namespace chrome_browser_metrics {
class TrackingSynchronizer;
}

namespace content {
struct MainFunctionParams;
}

namespace performance_monitor {
class StartupTimer;
}

class ChromeBrowserMainParts : public content::BrowserMainParts {
 public:
  virtual ~ChromeBrowserMainParts();

  // Add additional ChromeBrowserMainExtraParts.
  virtual void AddParts(ChromeBrowserMainExtraParts* parts);

 protected:
  explicit ChromeBrowserMainParts(
      const content::MainFunctionParams& parameters);

  // content::BrowserMainParts overrides.
  // These are called in-order by content::BrowserMainLoop.
  // Each stage calls the same stages in any ChromeBrowserMainExtraParts added
  // with AddParts() from ChromeContentBrowserClient::CreateBrowserMainParts.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PostEarlyInitialization() OVERRIDE;
  virtual void ToolkitInitialized() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE;

  // Additional stages for ChromeBrowserMainExtraParts. These stages are called
  // in order from PreMainMessageLoopRun(). See implementation for details.
  virtual void PreProfileInit();
  virtual void PostProfileInit();
  virtual void PreBrowserStart();
  virtual void PostBrowserStart();

#if !defined(OS_ANDROID)
  // Runs the PageCycler; called if the switch kVisitURLs is present.
  virtual void RunPageCycler();
#endif

  // Override this in subclasses to initialize platform specific field trials.
  virtual void SetupPlatformFieldTrials();

  // Displays a warning message that we can't find any locale data files.
  virtual void ShowMissingLocaleMessageBox() = 0;

  const content::MainFunctionParams& parameters() const {
    return parameters_;
  }
  const CommandLine& parsed_command_line() const {
    return parsed_command_line_;
  }

  Profile* profile() { return profile_; }

  const PrefService* local_state() const { return local_state_; }

 private:
  // Methods for |SetupMetricsAndFieldTrials()| --------------------------------

  // Constructs metrics service and does related initialization, including
  // creation of field trials. Call only after labs have been converted to
  // switches.
  void SetupMetricsAndFieldTrials();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  void StartMetricsRecording();

  // Returns true if the user opted in to sending metric reports.
  bool IsMetricsReportingEnabled();

  // Methods for Main Message Loop -------------------------------------------

  int PreCreateThreadsImpl();
  int PreMainMessageLoopRunImpl();

  // Members initialized on construction ---------------------------------------

  const content::MainFunctionParams& parameters_;
  const CommandLine& parsed_command_line_;
  int result_code_;

  // Create StartupTimeBomb object for watching jank during startup.
  scoped_ptr<StartupTimeBomb> startup_watcher_;

  // Create ShutdownWatcherHelper object for watching jank during shutdown.
  // Please keep |shutdown_watcher| as the first object constructed, and hence
  // it is destroyed last.
  scoped_ptr<ShutdownWatcherHelper> shutdown_watcher_;

  // A timer to hold data regarding startup and session restore times for
  // PerformanceMonitor so that we don't have to start the entire
  // PerformanceMonitor at browser startup.
  scoped_ptr<performance_monitor::StartupTimer> startup_timer_;

  // Creating this object starts tracking the creation and deletion of Task
  // instance. This MUST be done before main_message_loop, so that it is
  // destroyed after the main_message_loop.
  task_profiler::AutoTracking tracking_objects_;

  // Statistical testing infrastructure for the entire browser. NULL until
  // SetupMetricsAndFieldTrials is called.
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  ChromeBrowserFieldTrials browser_field_trials_;

  // Vector of additional ChromeBrowserMainExtraParts.
  // Parts are deleted in the inverse order they are added.
  std::vector<ChromeBrowserMainExtraParts*> chrome_extra_parts_;

  // Members initialized after / released before main_message_loop_ ------------

  scoped_ptr<BrowserProcessImpl> browser_process_;
  scoped_refptr<chrome_browser_metrics::TrackingSynchronizer>
      tracking_synchronizer_;
#if !defined(OS_ANDROID)
  // Browser creation happens on the Java side in Android.
  scoped_ptr<StartupBrowserCreator> browser_creator_;

  // Android doesn't support multiple browser processes, so it doesn't implement
  // ProcessSingleton.
  scoped_ptr<ProcessSingleton> process_singleton_;
#endif
  scoped_ptr<first_run::MasterPrefs> master_prefs_;
  bool record_search_engine_;
  TranslateManager* translate_manager_;
  Profile* profile_;
  bool run_message_loop_;
  ProcessSingleton::NotifyResult notify_result_;
  scoped_ptr<ThreeDAPIObserver> three_d_observer_;

  // Initialized in SetupMetricsAndFieldTrials.
  scoped_refptr<FieldTrialSynchronizer> field_trial_synchronizer_;

  // Members initialized in PreMainMessageLoopRun, needed in
  // PreMainMessageLoopRunThreadsCreated.
  bool do_first_run_tasks_;
  PrefService* local_state_;
  base::FilePath user_data_dir_;

  // Members needed across shutdown methods.
  bool restart_last_session_;

  // Tests can set this to true to disable restricting cookie access in the
  // network stack, as this can only be done once.
  static bool disable_enforcing_cookie_policies_for_tests_;

  friend class BrowserMainTest;
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest,
                           WarmConnectionFieldTrial_WarmestSocket);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Random);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Invalid);
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

// Records the time from our process' startup to the present time in
// the Startup.BrowserMessageLoopStartTime UMA histogram.
void RecordBrowserStartupTime();

// Records a time value to an UMA histogram in the context of the
// PreReadExperiment field-trial. This also reports to the appropriate
// sub-histogram (_PreRead(Enabled|Disabled)).
void RecordPreReadExperimentTime(const char* name, base::TimeDelta time);

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
