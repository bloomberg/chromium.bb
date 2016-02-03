// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/win/registry.h"
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
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace safe_browsing {

const wchar_t kSoftwareRemovalToolRegistryKey[] =
    L"Software\\Google\\Software Removal Tool";

const wchar_t kCleanerSubKey[] = L"Cleaner";

const wchar_t kEndTimeValueName[] = L"EndTime";
const wchar_t kStartTimeValueName[] = L"StartTime";

namespace {

// Used to send UMA information about missing start and end time registry values
// for the reporter.
enum SwReporterRunningTimeRegistryError {
  REPORTER_RUNNING_TIME_ERROR_NO_ERROR = 0,
  REPORTER_RUNNING_TIME_ERROR_REGISTRY_KEY_INVALID = 1,
  REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME = 2,
  REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME = 3,
  REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES = 4,
  REPORTER_RUNNING_TIME_ERROR_MAX,
};

const char kRunningTimeErrorMetricName[] =
    "SoftwareReporter.RunningTimeRegistryError";

// Overrides for the reporter launcher and prompt triggers free function, used
// by tests.
ReporterLauncher g_reporter_launcher_;
PromptTrigger g_prompt_trigger_;

const wchar_t kScanTimesSubKey[] = L"ScanTimes";
const wchar_t kFoundUwsValueName[] = L"FoundUws";
const wchar_t kMemoryUsedValueName[] = L"MemoryUsed";

const char kFoundUwsMetricName[] = "SoftwareReporter.FoundUwS";
const char kFoundUwsReadErrorMetricName[] =
    "SoftwareReporter.FoundUwSReadError";
const char kScanTimesMetricName[] = "SoftwareReporter.UwSScanTimes";
const char kMemoryUsedMetricName[] = "SoftwareReporter.MemoryUsed";

void DisplaySRTPrompt(const base::FilePath& download_path) {
  // Find the last active browser, which may be NULL, in which case we won't
  // show the prompt this time and will wait until the next run of the
  // reporter. We can't use other ways of finding a browser because we don't
  // have a profile.
  chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
  Browser* browser = chrome::FindLastActive();
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

// Reports UwS found by the software reporter tool via UMA and RAPPOR.
void ReportFoundUwS() {
  base::win::RegKey reporter_key(HKEY_CURRENT_USER,
                                 kSoftwareRemovalToolRegistryKey,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
  std::vector<base::string16> found_uws_strings;
  if (reporter_key.Valid() &&
      reporter_key.ReadValues(kFoundUwsValueName, &found_uws_strings) ==
          ERROR_SUCCESS) {
    rappor::RapporService* rappor_service = g_browser_process->rappor_service();

    bool parse_error = false;
    for (const base::string16& uws_string : found_uws_strings) {
      // All UwS ids are expected to be integers.
      uint32_t uws_id = 0;
      if (base::StringToUint(uws_string, &uws_id)) {
        UMA_HISTOGRAM_SPARSE_SLOWLY(kFoundUwsMetricName, uws_id);
        if (rappor_service) {
          rappor_service->RecordSample(kFoundUwsMetricName,
                                       rappor::COARSE_RAPPOR_TYPE,
                                       base::UTF16ToUTF8(uws_string));
        }
      } else {
        parse_error = true;
      }
    }

    // Clean up the old value.
    reporter_key.DeleteValue(kFoundUwsValueName);

    UMA_HISTOGRAM_BOOLEAN(kFoundUwsReadErrorMetricName, parse_error);
  }
}

// Reports to UMA the memory usage of the software reporter tool as reported by
// the tool itself in the Windows registry.
void ReportSwReporterMemoryUsage() {
  base::win::RegKey reporter_key(HKEY_CURRENT_USER,
                                 kSoftwareRemovalToolRegistryKey,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
  DWORD memory_used = 0;
  if (reporter_key.Valid() &&
      reporter_key.ReadValueDW(kMemoryUsedValueName, &memory_used) ==
          ERROR_SUCCESS) {
    UMA_HISTOGRAM_MEMORY_KB(kMemoryUsedMetricName, memory_used);
    reporter_key.DeleteValue(kMemoryUsedValueName);
  }
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
    variations::AppendVariationHeaders(
        url_fetcher_->GetOriginalURL(), io_data->IsOffTheRecord(),
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

  if (!incoming_seed.empty() && incoming_seed != old_seed) {
    prefs->SetString(prefs::kSwReporterPromptSeed, incoming_seed);
    // Forget about pending prompts if prompt seed has changed.
    if (local_state)
      local_state->SetBoolean(prefs::kSwReporterPendingPrompt, false);
  }
  prefs->SetString(prefs::kSwReporterPromptVersion, reporter_version);

  // Download the SRT.
  RecordReporterStepHistogram(SW_REPORTER_DOWNLOAD_START);

  // All the work happens in the self-deleting class below.
  new SRTFetcher(profile);
}

// Report the SwReporter run time with UMA both as reported by the tool via
// the registry and as measured by |ReporterRunner|.
void ReportSwReporterRuntime(const base::TimeDelta& reporter_running_time) {
  UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.RunningTimeAccordingToChrome",
                           reporter_running_time);

  base::win::RegKey reporter_key(
      HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey, KEY_ALL_ACCESS);
  if (!reporter_key.Valid()) {
    UMA_HISTOGRAM_ENUMERATION(kRunningTimeErrorMetricName,
                              REPORTER_RUNNING_TIME_ERROR_REGISTRY_KEY_INVALID,
                              REPORTER_RUNNING_TIME_ERROR_MAX);
    return;
  }

  bool has_start_time = false;
  int64_t start_time_value = 0;
  if (reporter_key.HasValue(kStartTimeValueName) &&
      reporter_key.ReadInt64(kStartTimeValueName, &start_time_value) ==
          ERROR_SUCCESS) {
    has_start_time = true;
    reporter_key.DeleteValue(kStartTimeValueName);
  }

  bool has_end_time = false;
  int64_t end_time_value = 0;
  if (reporter_key.HasValue(kEndTimeValueName) &&
      reporter_key.ReadInt64(kEndTimeValueName, &end_time_value) ==
          ERROR_SUCCESS) {
    has_end_time = true;
    reporter_key.DeleteValue(kEndTimeValueName);
  }

  if (has_start_time && has_end_time) {
    base::TimeDelta registry_run_time =
        base::Time::FromInternalValue(end_time_value) -
        base::Time::FromInternalValue(start_time_value);
    UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.RunningTime", registry_run_time);
    UMA_HISTOGRAM_ENUMERATION(kRunningTimeErrorMetricName,
                              REPORTER_RUNNING_TIME_ERROR_NO_ERROR,
                              REPORTER_RUNNING_TIME_ERROR_MAX);
  } else if (!has_start_time && !has_end_time) {
    UMA_HISTOGRAM_ENUMERATION(kRunningTimeErrorMetricName,
                              REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES,
                              REPORTER_RUNNING_TIME_ERROR_MAX);
  } else if (!has_start_time) {
    UMA_HISTOGRAM_ENUMERATION(kRunningTimeErrorMetricName,
                              REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME,
                              REPORTER_RUNNING_TIME_ERROR_MAX);
  } else {
    DCHECK(!has_end_time);
    UMA_HISTOGRAM_ENUMERATION(kRunningTimeErrorMetricName,
                              REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME,
                              REPORTER_RUNNING_TIME_ERROR_MAX);
  }
}

// Report the UwS scan times of the software reporter tool via UMA.
void ReportSwReporterScanTimes() {
  base::string16 scan_times_key_path = base::StringPrintf(
      L"%ls\\%ls", kSoftwareRemovalToolRegistryKey, kScanTimesSubKey);
  base::win::RegKey scan_times_key(HKEY_CURRENT_USER,
                                   scan_times_key_path.c_str(), KEY_ALL_ACCESS);
  if (!scan_times_key.Valid())
    return;

  base::string16 value_name;
  int uws_id = 0;
  int64_t raw_scan_time = 0;
  int num_scan_times = scan_times_key.GetValueCount();
  for (int i = 0; i < num_scan_times; ++i) {
    if (scan_times_key.GetValueNameAt(i, &value_name) == ERROR_SUCCESS &&
        base::StringToInt(value_name, &uws_id) &&
        scan_times_key.ReadInt64(value_name.c_str(), &raw_scan_time) ==
            ERROR_SUCCESS) {
      base::TimeDelta scan_time =
          base::TimeDelta::FromInternalValue(raw_scan_time);
      base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
          kScanTimesMetricName, base::HistogramBase::kUmaTargetedHistogramFlag);
      if (histogram) {
        // We report the number of seconds plus one because it can take less
        // than one second to scan some UwS and the count passed to |AddCount|
        // must be at least one.
        histogram->AddCount(uws_id, scan_time.InSeconds() + 1);
      }
    }
  }
  // Clean up by deleting the scan times key, which is a subkey of the main
  // reporter key.
  scan_times_key.Close();
  base::win::RegKey reporter_key(HKEY_CURRENT_USER,
                                 kSoftwareRemovalToolRegistryKey,
                                 KEY_ENUMERATE_SUB_KEYS);
  if (reporter_key.Valid())
    reporter_key.DeleteKey(kScanTimesSubKey);
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
  void ReporterDone(const base::Time& reporter_start_time, int exit_code) {
    DCHECK(thread_checker_.CalledOnValidThread());

    base::TimeDelta reporter_running_time =
        base::Time::Now() - reporter_start_time;
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
    ReportFoundUwS();
    PrefService* local_state = g_browser_process->local_state();
    if (local_state) {
      local_state->SetInteger(prefs::kSwReporterLastExitCode, exit_code);
      local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                            base::Time::Now().ToInternalValue());
    }
    ReportSwReporterRuntime(reporter_running_time);
    ReportSwReporterScanTimes();
    ReportSwReporterMemoryUsage();

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
    Browser* browser = chrome::FindLastActive();
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
      RecordReporterStepHistogram(SW_REPORTER_RAN_DAILY);
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
          base::Bind(&ReporterRunner::ReporterDone, base::Unretained(this),
                     base::Time::Now()));
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

bool ReporterFoundUws() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return false;
  int exit_code = local_state->GetInteger(prefs::kSwReporterLastExitCode);
  return exit_code == kSwReporterCleanupNeeded;
}

bool UserHasRunCleaner() {
  base::string16 cleaner_key_path(kSoftwareRemovalToolRegistryKey);
  cleaner_key_path.append(L"\\").append(kCleanerSubKey);

  base::win::RegKey srt_cleaner_key(HKEY_CURRENT_USER, cleaner_key_path.c_str(),
                                    KEY_QUERY_VALUE);

  return srt_cleaner_key.Valid() && srt_cleaner_key.GetValueCount() > 0;
}

void SetReporterLauncherForTesting(const ReporterLauncher& reporter_launcher) {
  g_reporter_launcher_ = reporter_launcher;
}

void SetPromptTriggerForTesting(const PromptTrigger& prompt_trigger) {
  g_prompt_trigger_ = prompt_trigger;
}

}  // namespace safe_browsing
