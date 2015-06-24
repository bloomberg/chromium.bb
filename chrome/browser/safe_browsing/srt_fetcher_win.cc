// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/component_updater/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// The number of days to wait before triggering another sw_reporter run.
const int kDaysBetweenSuccessfulSwReporterRuns = 7;

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

  // Do not try to show bubble if another GlobalError is already showing
  // one. The bubble will be shown once the others have been dismissed.
  bool need_to_show_bubble = true;
  for (GlobalError* error : global_error_service->errors()) {
    if (error->GetBubbleView()) {
      need_to_show_bubble = false;
      break;
    }
  }
  if (need_to_show_bubble)
    global_error->ShowBubbleView(browser);
}

// Class that will attempt to download the SRT, showing the SRT notification
// bubble when the download operation is complete. Instances of SRTFetcher own
// themselves, they will self-delete on completion of the network request when
// OnURLFetchComplete is called.
class SRTFetcher : public net::URLFetcherDelegate {
 public:
  SRTFetcher()
      : url_fetcher_(net::URLFetcher::Create(0,
                                             GURL(GetSRTDownloadURL()),
                                             net::URLFetcher::GET,
                                             this)) {
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetMaxRetriesOn5xx(3);
    url_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
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

  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SRTFetcher);
};

// This class tries to run the reporter and reacts to its exit code. It
// schedules subsequent runs as needed, or retries as soon as a browser is
// available when none is on first try.
class ReporterRunner : public chrome::BrowserListObserver {
 public:
  static ReporterRunner* GetInstance() {
    return Singleton<ReporterRunner,
                     LeakySingletonTraits<ReporterRunner>>::get();
  }

  // Starts the sequence of attempts to run the reporter.
  void Run(const base::FilePath& exe_path, const std::string& version) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    exe_path_ = exe_path;
    version_ = version;
    TryToRun();
  }

 private:
  friend struct DefaultSingletonTraits<ReporterRunner>;
  ReporterRunner() {}
  ~ReporterRunner() override {}

  // BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {}
  void OnBrowserAdded(Browser* browser) override {
    DCHECK(browser);
    if (browser->host_desktop_type() == chrome::GetActiveDesktop()) {
      MaybeFetchSRT(browser);
      BrowserList::RemoveObserver(this);
    }
  }

  // Identifies that we completed the run of the reporter and schedule the next
  // run in kDaysBetweenSuccessfulSwReporterRuns.
  void CompletedRun() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    g_browser_process->local_state()->SetInt64(
        prefs::kSwReporterLastTimeTriggered,
        base::Time::Now().ToInternalValue());

    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ReporterRunner::TryToRun, base::Unretained(this)),
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
  }

  // This method is called on the UI thread when the reporter run has completed.
  // This is run as a task posted from an interruptible worker thread so should
  // be resilient to unexpected shutdown.
  void ReporterDone(int exit_code) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.ExitCode", exit_code);

    if (g_browser_process && g_browser_process->local_state()) {
      g_browser_process->local_state()->SetInteger(
          prefs::kSwReporterLastExitCode, exit_code);
    }

    // To complete the run if we exit without releasing this scoped closure.
    base::ScopedClosureRunner completed(
        base::Bind(&ReporterRunner::CompletedRun, base::Unretained(this)));

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
      BrowserList::AddObserver(GetInstance());
    } else {
      MaybeFetchSRT(browser);
    }
  }

  static void MaybeFetchSRT(Browser* browser) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    Profile* profile = browser->profile();
    DCHECK(profile);
    PrefService* prefs = profile->GetPrefs();
    DCHECK(prefs);

    // Don't show the prompt again if it's been shown before for this profile
    // and for the current Finch seed.
    std::string incoming_seed = GetIncomingSRTSeed();
    std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
    if (!incoming_seed.empty() && incoming_seed == old_seed) {
      RecordReporterStepHistogram(SW_REPORTER_ALREADY_PROMPTED);
      return;
    }

    if (!incoming_seed.empty())
      prefs->SetString(prefs::kSwReporterPromptSeed, incoming_seed);
    prefs->SetString(prefs::kSwReporterPromptVersion, GetInstance()->version_);

    // Download the SRT.
    RecordReporterStepHistogram(SW_REPORTER_DOWNLOAD_START);

    // All the work happens in the self-deleting class above.
    new SRTFetcher();
  }

  // This method is called from a worker thread to launch the SwReporter and
  // wait for termination to collect its exit code. This task could be
  // interrupted by a shutdown at anytime, so it shouldn't depend on anything
  // external that could be shutdown beforehand. It's static to make sure it
  // won't access data members, except for the need for a this pointer to call
  // back member functions on the UI thread. We can safely use GetInstance()
  // here because the singleton is leaky.
  static void LaunchAndWaitForExit(const base::FilePath& exe_path,
                                   const std::string& version) {
    const base::CommandLine reporter_command_line(exe_path);
    base::Process reporter_process =
        base::LaunchProcess(reporter_command_line, base::LaunchOptions());
    if (!reporter_process.IsValid()) {
      RecordReporterStepHistogram(SW_REPORTER_FAILED_TO_START);
      // Don't call ReporterDone when the reporter process failed to launch, but
      // try again after the regular delay. It's not worth retrying earlier,
      // risking running too often if it always fail, since not many users fail
      // here, less than 1%.
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ReporterRunner::TryToRun,
                     base::Unretained(GetInstance())),
          base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns));
      return;
    }
    RecordReporterStepHistogram(SW_REPORTER_START_EXECUTION);

    int exit_code = -1;
    bool success = reporter_process.WaitForExit(&exit_code);
    DCHECK(success);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ReporterRunner::ReporterDone,
                   base::Unretained(GetInstance()), exit_code));
  }

  void TryToRun() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (version_.empty()) {
      DCHECK(exe_path_.empty());
      return;
    }

    // Run the reporter if it hasn't been triggered in the
    // kDaysBetweenSuccessfulSwReporterRuns days.
    const base::Time last_time_triggered = base::Time::FromInternalValue(
        g_browser_process->local_state()->GetInt64(
            prefs::kSwReporterLastTimeTriggered));
    base::TimeDelta next_trigger_delay(
        last_time_triggered +
        base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns) -
        base::Time::Now());
    if (next_trigger_delay.ToInternalValue() <= 0) {
      // Use a worker pool because this task is blocking and may take a long
      // time to complete.
      base::WorkerPool::PostTask(
          FROM_HERE, base::Bind(&ReporterRunner::LaunchAndWaitForExit,
                                exe_path_, version_),
          true);
    } else {
      BrowserThread::PostDelayedTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ReporterRunner::TryToRun,
                     base::Unretained(GetInstance())),
          next_trigger_delay);
    }
  }

  base::FilePath exe_path_;
  std::string version_;

  DISALLOW_COPY_AND_ASSIGN(ReporterRunner);
};

}  // namespace

void RunSwReporter(const base::FilePath& exe_path, const std::string& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ReporterRunner::GetInstance()->Run(exe_path, version);
}

}  // namespace safe_browsing
