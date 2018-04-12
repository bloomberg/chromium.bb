// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/chrome_process_singleton.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/thread_profiler.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

class BrowserProcessImpl;
class ChromeBrowserMainExtraParts;
class FieldTrialSynchronizer;
class PrefService;
class Profile;
class StartupBrowserCreator;
class StartupTimeBomb;
class ShutdownWatcherHelper;
class WebUsbDetector;

namespace base {
class SequencedTaskRunner;
}

namespace chrome_browser {
// For use by ShowMissingLocaleMessageBox.
#if defined(OS_WIN)
extern const char kMissingLocaleDataTitle[];
#endif

#if defined(OS_WIN)
extern const char kMissingLocaleDataMessage[];
#endif
}

class ChromeBrowserMainParts : public content::BrowserMainParts {
 public:
  ~ChromeBrowserMainParts() override;

  // Add additional ChromeBrowserMainExtraParts.
  virtual void AddParts(ChromeBrowserMainExtraParts* parts);

 protected:
#if !defined(OS_ANDROID)
  class DeferringTaskRunner;
#endif

  explicit ChromeBrowserMainParts(
      const content::MainFunctionParams& parameters);

  // content::BrowserMainParts overrides.
  bool ShouldContentCreateFeatureList() override;
  // These are called in-order by content::BrowserMainLoop.
  // Each stage calls the same stages in any ChromeBrowserMainExtraParts added
  // with AddParts() from ChromeContentBrowserClient::CreateBrowserMainParts.
  int PreEarlyInitialization() override;
  void PostEarlyInitialization() override;
  void ToolkitInitialized() override;
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PreShutdown() override;
  void PostDestroyThreads() override;

  // Additional stages for ChromeBrowserMainExtraParts. These stages are called
  // in order from PreMainMessageLoopRun(). See implementation for details.
  virtual void PreProfileInit();
  virtual void PostProfileInit();
  virtual void PreBrowserStart();
  virtual void PostBrowserStart();

  // Displays a warning message that we can't find any locale data files.
  virtual void ShowMissingLocaleMessageBox() = 0;

  const content::MainFunctionParams& parameters() const {
    return parameters_;
  }
  const base::CommandLine& parsed_command_line() const {
    return parsed_command_line_;
  }
  const base::FilePath& user_data_dir() const {
    return user_data_dir_;
  }

  Profile* profile() { return profile_; }

 private:
  friend class ChromeBrowserMainPartsTestApi;

  // Sets up the field trials and related initialization. Call only after
  // about:flags have been converted to switches.
  void SetupFieldTrials();

  // Constructs the metrics service and initializes metrics recording.
  void SetupMetrics();

  // Starts recording of metrics. This can only be called after we have a file
  // thread.
  void StartMetricsRecording();

  // Record time from process startup to present time in an UMA histogram.
  void RecordBrowserStartupTime();

  // Reads origin trial policy data from local state and configures command line
  // for child processes.
  void SetupOriginTrialsCommandLine(PrefService* local_state);

  // Calling during PreEarlyInitialization() to load local state. Return value
  // is an exit status, RESULT_CODE_NORMAL_EXIT indicates success.
  // If the return value is RESULT_CODE_MISSING_DATA, then
  // |failed_to_load_resource_bundle| indicates if the ResourceBundle couldn't
  // be loaded.
  int LoadLocalState(base::SequencedTaskRunner* local_state_task_runner,
                     bool* failed_to_load_resource_bundle);

  // Applies any preferences (to local state) needed for first run. This is
  // always called and early outs if not first-run. Return value is an exit
  // status, RESULT_CODE_NORMAL_EXIT indicates success.
  int ApplyFirstRunPrefs();

  // Methods for Main Message Loop -------------------------------------------

  int PreCreateThreadsImpl();
  int PreMainMessageLoopRunImpl();

  // Members initialized on construction ---------------------------------------

  const content::MainFunctionParams parameters_;
  // TODO(sky): remove this. This class (and related calls), may mutate the
  // CommandLine, so it is misleading keeping a const ref here.
  const base::CommandLine& parsed_command_line_;
  int result_code_;

  // Create StartupTimeBomb object for watching jank during startup.
  std::unique_ptr<StartupTimeBomb> startup_watcher_;

  // Create ShutdownWatcherHelper object for watching jank during shutdown.
  // Please keep |shutdown_watcher| as the first object constructed, and hence
  // it is destroyed last.
  std::unique_ptr<ShutdownWatcherHelper> shutdown_watcher_;

  ChromeBrowserFieldTrials browser_field_trials_;

#if !defined(OS_ANDROID)
  std::unique_ptr<WebUsbDetector> web_usb_detector_;
#endif

  // Vector of additional ChromeBrowserMainExtraParts.
  // Parts are deleted in the inverse order they are added.
  std::vector<ChromeBrowserMainExtraParts*> chrome_extra_parts_;

  // A profiler that periodically samples stack traces on the UI thread.
  std::unique_ptr<ThreadProfiler> ui_thread_profiler_;

  // Whether PerformPreMainMessageLoopStartup() is called on VariationsService.
  // Initialized to true if |MainFunctionParams::ui_task| is null (meaning not
  // running browser_tests), but may be forced to true for tests.
  bool should_call_pre_main_loop_start_startup_on_variations_service_;

  // Members initialized after / released before main_message_loop_ ------------

  std::unique_ptr<BrowserProcessImpl> browser_process_;

#if !defined(OS_ANDROID)
  // Browser creation happens on the Java side in Android.
  std::unique_ptr<StartupBrowserCreator> browser_creator_;

  // Android doesn't support multiple browser processes, so it doesn't implement
  // ProcessSingleton.
  std::unique_ptr<ChromeProcessSingleton> process_singleton_;

  // Android's first run is done in Java instead of native.
  std::unique_ptr<first_run::MasterPrefs> master_prefs_;

  ProcessSingleton::NotifyResult notify_result_ =
      ProcessSingleton::PROCESS_NONE;

  // Members needed across shutdown methods.
  bool restart_last_session_ = false;
#endif

  Profile* profile_;
  bool run_message_loop_;

  // Initialized in |SetupFieldTrials()|.
  scoped_refptr<FieldTrialSynchronizer> field_trial_synchronizer_;

  base::FilePath user_data_dir_;

#if !defined(OS_ANDROID)
  // This TaskRunner is created and the constructor and destroyed in
  // PreCreateThreadsImpl(). It's used to queue any tasks scheduled before the
  // real task scheduler has been created.
  scoped_refptr<DeferringTaskRunner> initial_task_runner_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainParts);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_H_
