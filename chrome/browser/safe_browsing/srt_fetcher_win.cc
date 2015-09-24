// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/component_updater/pref_names.h"
#include "components/variations/net/variations_http_header_provider.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// Overrides for the reporter launcher and prompt triggers free function, used
// by tests.
ReporterLauncher g_reporter_launcher_;
PromptTrigger g_prompt_trigger_;

void DisplaySRTPrompt(const base::FilePath& download_path) {
  // Find the last active browser, which may be NULL, in which case we won't
  // show the prompt this time and will wait until the next run of the
  // reporter. We can't use other ways of finding a browser because we don't
  // have a profile.
  chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(desktop_type);
  if (!browser)
    return;

  Profile* profile = browser->profile();
  DCHECK(profile);

  // Make sure we have a tabbed browser since we need to anchor the bubble to
  // the toolbar's wrench menu. Create one if none exist already.
  if (browser->type() != Browser::TYPE_TABBED) {
    browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (!browser)
      browser = new Browser(Browser::CreateParams(profile, desktop_type));
  }
  GlobalErrorService* global_error_service =
      GlobalErrorServiceFactory::GetForProfile(profile);
  SRTGlobalError* global_error =
      new SRTGlobalError(global_error_service, download_path);

  // Ownership of |global_error| is passed to the service. The error removes
  // itself from the service and self-destructs when done.
  global_error_service->AddGlobalError(global_error);

  bool show_bubble = true;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state && local_state->GetBoolean(prefs::kSwReporterPendingPrompt)) {
    // Don't show the bubble if there's already a pending prompt to only be
    // sown in the Chrome menu.
    RecordReporterStepHistogram(SW_REPORTER_ADDED_TO_MENU);
    show_bubble = false;
  } else {
    // Do not try to show bubble if another GlobalError is already showing
    // one. The bubble will be shown once the others have been dismissed.
    for (GlobalError* error : global_error_service->errors()) {
      if (error->GetBubbleView()) {
        show_bubble = false;
        break;
      }
    }
  }
  if (show_bubble)
    global_error->ShowBubbleView(browser);
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be
// interrupted by a shutdown at any time, so it shouldn't depend on anything
// external that could be shut down beforehand.
int LaunchAndWaitForExit(const base::FilePath& exe_path,
                         const std::string& version) {
  if (!g_reporter_launcher_.is_null())
    return g_reporter_launcher_.Run(exe_path, version);
  base::Process reporter_process =
      base::LaunchProcess(exe_path.value(), base::LaunchOptions());
  // This exit code is used to identify that a reporter run didn't happen, so
  // the result should be ignored and a rerun scheduled for the usual delay.
  int exit_code = kReporterFailureExitCode;
  if (reporter_process.IsValid()) {
    RecordReporterStepHistogram(SW_REPORTER_START_EXECUTION);
    bool success = reporter_process.WaitForExit(&exit_code);
    DCHECK(success);
  } else {
    RecordReporterStepHistogram(SW_REPORTER_FAILED_TO_START);
  }
  return exit_code;
}

}  // namespace

// Class that will attempt to download the SRT, showing the SRT notification
// bubble when the download operation is complete. Instances of SRTFetcher own
// themselves, they will self-delete on completion of the network request when
// OnURLFetchComplete is called.
class SRTFetcher : public net::URLFetcherDelegate {
 public:
  explicit SRTFetcher(Profile* profile)
      : profile_(profile),
        url_fetcher_(net::URLFetcher::Create(0,
                                             GURL(GetSRTDownloadURL()),
                                             net::URLFetcher::GET,
                                             this)) {
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetMaxRetriesOn5xx(3);
    url_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    // Adds the UMA bit to the download request if the user is enrolled in UMA.
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(
        profile_->GetResourceContext());
    net::HttpRequestHeaders headers;
    variations::VariationsHttpHeaderProvider::GetInstance()->AppendHeaders(
        url_fetcher_->GetOriginalURL(),
        io_data->IsOffTheRecord(),
        ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(),
        &headers);
    url_fetcher_->SetExtraRequestHeaders(headers.ToString());
    url_fetcher_->Start();
  }

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    // Take ownership of the fetcher in this scope (source == url_fetcher_).
    DCHECK_EQ(url_fetcher_.get(), source);

    base::FilePath download_path;
    if (source->GetStatus().is_success() &&
        source->GetResponseCode() == net::HTTP_OK) {
      if (source->GetResponseAsFilePath(true, &download_path)) {
        DCHECK(!download_path.empty());
      }
    }

    // As long as the fetch didn't fail due to HTTP_NOT_FOUND, show a prompt
    // (either offering the tool directly or pointing to the download page).
    // If the fetch failed to find the file, don't prompt the user since the
    // tool is not currently available.
    // TODO(mad): Consider implementing another layer of retries / alternate
    //            fetching mechanisms. http://crbug.com/460293
    // TODO(mad): In the event the browser is closed before the prompt displays,
    //            we will wait until the next scanner run to re-display it.
    //            Improve this. http://crbug.com/460295
    if (source->GetResponseCode() != net::HTTP_NOT_FOUND)
      DisplaySRTPrompt(download_path);
    else
      RecordSRTPromptHistogram(SRT_PROMPT_DOWNLOAD_UNAVAILABLE);

    // Explicitly destroy the url_fetcher_ to avoid destruction races.
    url_fetcher_.reset();

    // At this point, the url_fetcher_ is gone and this SRTFetcher instance is
    // no longer needed.
    delete this;
  }

 private:
  ~SRTFetcher() override {}

  // The user profile.
  Profile* profile_;

  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SRTFetcher);
};

namespace {

// Try to fetch the SRT, and on success, show the prompt to run it.
void MaybeFetchSRT(Browser* browser, const std::string& reporter_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_prompt_trigger_.is_null()) {
    g_prompt_trigger_.Run(browser, reporter_version);
    return;
  }
  Profile* profile = browser->profile();
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Don't show the prompt again if it's been shown before for this profile
  // and for the current variations seed, unless there's a pending prompt to
  // show in the Chrome menu.
  std::string incoming_seed = GetIncomingSRTSeed();
  std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
  PrefService* local_state = g_browser_process->local_state();
  bool pending_prompt =
      local_state && local_state->GetBoolean(prefs::kSwReporterPendingPrompt);
  if (!incoming_seed.empty() && incoming_seed == old_seed && !pending_prompt) {
    RecordReporterStepHistogram(SW_REPORTER_ALREADY_PROMPTED);
    return;
  }

  if (!incoming_seed.empty())
    prefs->SetString(prefs::kSwReporterPromptSeed, incoming_seed);
  prefs->SetString(prefs::kSwReporterPromptVersion, reporter_version);

  // Download the SRT.
  RecordReporterStepHistogram(SW_REPORTER_DOWNLOAD_START);

  // All the work happens in the self-deleting class below.
  new SRTFetcher(profile);
}

// This class tries to run the reporter and reacts to its exit code. It
// schedules subsequent runs as needed, or retries as soon as a browser is
// available when none is on first try.
class ReporterRunner : public chrome::BrowserListObserver {
 public:
  // Starts the sequence of attempts to run the reporter.
  static void Run(
      const base::FilePath& exe_path,
      const std::string& version,
      const scoped_refptr<base::TaskRunner>& main_thread_task_runner,
      const scoped_refptr<base::TaskRunner>& blocking_task_runner) {
    if (!instance_)
      instance_ = new ReporterRunner;
    DCHECK(instance_->thread_checker_.CalledOnValidThread());
    // There's nothing to do if the path and version of the reporter has not
    // changed, we just keep running the tasks that are running now.
    if (instance_->exe_path_ == exe_path && instance_->version_ == version)
      return;
    bool first_run = instance_->exe_path_.empty();

    instance_->exe_path_ = exe_path;
    instance_->version_ = version;
    instance_->main_thread_task_runner_ = main_thread_task_runner;
    instance_->blocking_task_runner_ = blocking_task_runner;

    if (first_run)
      instance_->TryToRun();
  }

 private:
  ReporterRunner() {}
  ~ReporterRunner() override {}

  // BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {}
  void OnBrowserAdded(Browser* browser) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(browser);
    if (browser->host_desktop_type() == chrome::GetActiveDesktop()) {
      MaybeFetchSRT(browser, version_);
      BrowserList::RemoveObserver(this);
    }
  }

  // This method is called on the UI thread when the reporter run has completed.
  // This is run as a task posted from an interruptible worker thread so should
  // be resilient to unexpected shutdown.
  void ReporterDone(int exit_code) {
    DCHECK(thread_checker_.CalledOnValidThread());

    // Don't continue when the reporter process failed to launch, but still try
    // again after the regular delay. It's not worth retrying earlier, risking
    // running too often if it always fails, since not many users fail here.
    main_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ReporterRunner::TryToRun, base::Unretained(this)),
        base::TimeDelta::FromDays(days_between_reporter_runs_));
    if (exit_code == kReporterFailureExitCode)
      return;

    UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.ExitCode", exit_code);
    PrefService* local_state = g_browser_process->local_state();
    if (local_state) {
      local_state->SetInteger(prefs::kSwReporterLastExitCode, exit_code);
      local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                            base::Time::Now().ToInternalValue());
    }

    if (!IsInSRTPromptFieldTrialGroups()) {
      // Knowing about disabled field trial is more important than reporter not
      // finding anything to remove, so check this case first.
      RecordReporterStepHistogram(SW_REPORTER_NO_PROMPT_FIELD_TRIAL);
      return;
    }

    if (exit_code != kSwReporterPostRebootCleanupNeeded &&
        exit_code != kSwReporterCleanupNeeded) {
      RecordReporterStepHistogram(SW_REPORTER_NO_PROMPT_NEEDED);
      return;
    }

    // Find the last active browser, which may be NULL, in which case we need
    // to wait for one to be available. We can't use other ways of finding a
    // browser because we don't have a profile. And we need a browser to get to
    // a profile, which we need, to tell whether we should prompt or not.
    // TODO(mad): crbug.com/503269, investigate whether we should change how we
    // decide when it's time to download the SRT and when to display the prompt.
    chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
    Browser* browser = chrome::FindLastActiveWithHostDesktopType(desktop_type);
    if (!browser) {
      RecordReporterStepHistogram(SW_REPORTER_NO_BROWSER);
      BrowserList::AddObserver(this);
    } else {
      MaybeFetchSRT(browser, version_);
    }
  }

  void TryToRun() {
    DCHECK(thread_checker_.CalledOnValidThread());
    PrefService* local_state = g_browser_process->local_state();
    if (version_.empty() || !local_state) {
      DCHECK(exe_path_.empty());
      return;
    }

    // Run the reporter if it hasn't been triggered in the last
    // |days_between_reporter_runs_| days, which depends if there is a pending
    // prompt to be added to Chrome's menu.
    if (local_state->GetBoolean(prefs::kSwReporterPendingPrompt)) {
      days_between_reporter_runs_ = kDaysBetweenSwReporterRunsForPendingPrompt;
      safe_browsing::RecordReporterStepHistogram(
          safe_browsing::SW_REPORTER_RAN_DAILY);
    } else {
      days_between_reporter_runs_ = kDaysBetweenSuccessfulSwReporterRuns;
    }
    const base::Time last_time_triggered = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kSwReporterLastTimeTriggered));
    base::TimeDelta next_trigger_delay(
        last_time_triggered +
        base::TimeDelta::FromDays(days_between_reporter_runs_) -
        base::Time::Now());
    if (next_trigger_delay.ToInternalValue() <= 0 ||
        // Also make sure the kSwReporterLastTimeTriggered value is not set in
        // the future.
        last_time_triggered > base::Time::Now()) {
      // It's OK to simply |PostTaskAndReplyWithResult| so that
      // |LaunchAndWaitForExit| doesn't need to access
      // |main_thread_task_runner_| since the callback is not delayed and the
      // test task runner won't need to force it.
      base::PostTaskAndReplyWithResult(
          blocking_task_runner_.get(), FROM_HERE,
          base::Bind(&LaunchAndWaitForExit, exe_path_, version_),
          base::Bind(&ReporterRunner::ReporterDone, base::Unretained(this)));
    } else {
      main_thread_task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ReporterRunner::TryToRun, base::Unretained(this)),
          next_trigger_delay);
    }
  }

  base::FilePath exe_path_;
  std::string version_;
  scoped_refptr<base::TaskRunner> main_thread_task_runner_;
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  // This value is used to identify how long to wait before starting a new run
  // of the reporter. It's initialized with the default value and may be changed
  // to a different value when a prompt is pending and the reporter should be
  // run before adding the global error to the Chrome menu.
  int days_between_reporter_runs_ = kDaysBetweenSuccessfulSwReporterRuns;

  // A single leaky instance.
  static ReporterRunner* instance_;

  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(ReporterRunner);
};

ReporterRunner* ReporterRunner::instance_ = nullptr;

}  // namespace

void RunSwReporter(
    const base::FilePath& exe_path,
    const std::string& version,
    const scoped_refptr<base::TaskRunner>& main_thread_task_runner,
    const scoped_refptr<base::TaskRunner>& blocking_task_runner) {
  ReporterRunner::Run(exe_path, version, main_thread_task_runner,
                      blocking_task_runner);
}

void SetReporterLauncherForTesting(const ReporterLauncher& reporter_launcher) {
  g_reporter_launcher_ = reporter_launcher;
}

void SetPromptTriggerForTesting(const PromptTrigger& prompt_trigger) {
  g_prompt_trigger_ = prompt_trigger;
}

}  // namespace safe_browsing
