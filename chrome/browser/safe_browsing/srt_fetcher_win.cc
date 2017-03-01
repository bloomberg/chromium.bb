// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_fetcher_win.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/safe_browsing/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace safe_browsing {

// TODO(b/647763) Change the registry key to properly handle cases when the user
// runs Google Chrome stable alongside Google Chrome SxS.
const wchar_t kSoftwareRemovalToolRegistryKey[] =
    L"Software\\Google\\Software Removal Tool";

const wchar_t kCleanerSubKey[] = L"Cleaner";

const wchar_t kEndTimeValueName[] = L"EndTime";
const wchar_t kStartTimeValueName[] = L"StartTime";

namespace {

// Used to send UMA information about missing start and end time registry
// values for the reporter. Replicated in the histograms.xml file, so the order
// MUST NOT CHANGE.
enum SwReporterRunningTimeRegistryError {
  REPORTER_RUNNING_TIME_ERROR_NO_ERROR = 0,
  REPORTER_RUNNING_TIME_ERROR_REGISTRY_KEY_INVALID = 1,
  REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME = 2,
  REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME = 3,
  REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES = 4,
  REPORTER_RUNNING_TIME_ERROR_MAX,
};

// Used to send UMA information about the progress of the SwReporter launch and
// prompt sequence. Replicated in the histograms.xml file, so the order MUST
// NOT CHANGE.
enum SwReporterUmaValue {
  // Deprecated.
  SW_REPORTER_EXPLICIT_REQUEST = 0,
  // Deprecated.
  SW_REPORTER_STARTUP_RETRY = 1,
  // Deprecated.
  SW_REPORTER_RETRIED_TOO_MANY_TIMES = 2,
  SW_REPORTER_START_EXECUTION = 3,
  SW_REPORTER_FAILED_TO_START = 4,
  // Deprecated.
  SW_REPORTER_REGISTRY_EXIT_CODE = 5,
  // Deprecated.
  SW_REPORTER_RESET_RETRIES = 6,
  SW_REPORTER_DOWNLOAD_START = 7,
  SW_REPORTER_NO_BROWSER = 8,
  SW_REPORTER_NO_LOCAL_STATE = 9,
  SW_REPORTER_NO_PROMPT_NEEDED = 10,
  SW_REPORTER_NO_PROMPT_FIELD_TRIAL = 11,
  SW_REPORTER_ALREADY_PROMPTED = 12,
  SW_REPORTER_RAN_DAILY = 13,
  SW_REPORTER_ADDED_TO_MENU = 14,

  SW_REPORTER_MAX,
};

// Used to send UMA information showing whether uploading of Software Reporter
// logs is enabled, or the reason why not.
// Replicated in the histograms.xml file, so the order MUST NOT CHANGE.
enum SwReporterLogsUploadsEnabled {
  REPORTER_LOGS_UPLOADS_ENABLED = 0,
  REPORTER_LOGS_UPLOADS_SBER_DISABLED = 1,
  REPORTER_LOGS_UPLOADS_RECENTLY_SENT_LOGS = 2,
  REPORTER_LOGS_UPLOADS_MAX,
};

// Used to send UMA information about missing logs upload result in the registry
// for the reporter. Replicated in the histograms.xml file, so the order
// MUST NOT CHANGE.
enum SwReporterLogsUploadResultRegistryError {
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_NO_ERROR = 0,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_REGISTRY_KEY_INVALID = 1,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_NOT_FOUND = 2,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_OUT_OF_BOUNDS = 3,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX,
};

const char kRunningTimeErrorMetricName[] =
    "SoftwareReporter.RunningTimeRegistryError";

SwReporterTestingDelegate* g_testing_delegate_ = nullptr;

const wchar_t kScanTimesSubKey[] = L"ScanTimes";
const wchar_t kFoundUwsValueName[] = L"FoundUws";
const wchar_t kMemoryUsedValueName[] = L"MemoryUsed";
const wchar_t kLogsUploadResultValueName[] = L"LogsUploadResult";
const wchar_t kExitCodeValueName[] = L"ExitCode";
const wchar_t kEngineErrorCodeValueName[] = L"EngineErrorCode";

const char kFoundUwsMetricName[] = "SoftwareReporter.FoundUwS";
const char kFoundUwsReadErrorMetricName[] =
    "SoftwareReporter.FoundUwSReadError";
const char kScanTimesMetricName[] = "SoftwareReporter.UwSScanTimes";
const char kMemoryUsedMetricName[] = "SoftwareReporter.MemoryUsed";
const char kStepMetricName[] = "SoftwareReporter.Step";
const char kLogsUploadEnabledMetricName[] =
    "SoftwareReporter.LogsUploadEnabled";
const char kLogsUploadResultMetricName[] = "SoftwareReporter.LogsUploadResult";
const char kLogsUploadResultRegistryErrorMetricName[] =
    "SoftwareReporter.LogsUploadResultRegistryError";
const char kExitCodeMetricName[] = "SoftwareReporter.ExitCodeFromRegistry";
const char kEngineErrorCodeMetricName[] = "SoftwareReporter.EngineErrorCode";

// The max value for histogram SoftwareReporter.LogsUploadResult, which is used
// to send UMA information about the result of Software Reporter's attempt to
// upload logs, when logs are enabled. This value must be consistent with the
// SoftwareReporterLogsUploadResult enum defined in the histograms.xml file.
const int kSwReporterLogsUploadResultMax = 30;

// Reports metrics about the software reporter via UMA (and sometimes Rappor).
class UMAHistogramReporter {
 public:
  UMAHistogramReporter() : UMAHistogramReporter(std::string()) {}

  explicit UMAHistogramReporter(const std::string& suffix)
      : suffix_(suffix),
        registry_key_(suffix.empty() ? kSoftwareRemovalToolRegistryKey
                                     : base::StringPrintf(
                                           L"%ls\\%ls",
                                           kSoftwareRemovalToolRegistryKey,
                                           base::UTF8ToUTF16(suffix).c_str())) {
  }

  // Reports the software reporter tool's version via UMA.
  void ReportVersion(const base::Version& version) const {
    DCHECK(!version.components().empty());
    // The minor version is the 2nd last component of the version,
    // or just the first component if there is only 1.
    uint32_t minor_version = 0;
    if (version.components().size() > 1)
      minor_version = version.components()[version.components().size() - 2];
    else
      minor_version = version.components()[0];
    RecordSparseHistogram("SoftwareReporter.MinorVersion", minor_version);

    // The major version for X.Y.Z is X*256^3+Y*256+Z. If there are additional
    // components, only the first three count, and if there are less than 3, the
    // missing values are just replaced by zero. So 1 is equivalent 1.0.0.
    DCHECK_LT(version.components()[0], 0x100U);
    uint32_t major_version = 0x1000000 * version.components()[0];
    if (version.components().size() >= 2) {
      DCHECK_LT(version.components()[1], 0x10000U);
      major_version += 0x100 * version.components()[1];
    }
    if (version.components().size() >= 3) {
      DCHECK_LT(version.components()[2], 0x100U);
      major_version += version.components()[2];
    }
    RecordSparseHistogram("SoftwareReporter.MajorVersion", major_version);
  }

  void ReportExitCode(int exit_code) const {
    RecordSparseHistogram("SoftwareReporter.ExitCode", exit_code);

    // Also report the exit code that the reporter writes to the registry.
    base::win::RegKey reporter_key;
    DWORD exit_code_in_registry;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(kExitCodeValueName, &exit_code_in_registry) !=
            ERROR_SUCCESS) {
      return;
    }

    RecordSparseHistogram(kExitCodeMetricName, exit_code_in_registry);
    reporter_key.DeleteValue(kExitCodeValueName);
  }

  void ReportEngineErrorCode() const {
    base::win::RegKey reporter_key;
    DWORD engine_error_code;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(kEngineErrorCodeValueName,
                                 &engine_error_code) != ERROR_SUCCESS) {
      return;
    }

    RecordSparseHistogram(kEngineErrorCodeMetricName, engine_error_code);
    reporter_key.DeleteValue(kEngineErrorCodeValueName);
  }

  // Reports UwS found by the software reporter tool via UMA and RAPPOR.
  void ReportFoundUwS(bool use_rappor) const {
    base::win::RegKey reporter_key;
    std::vector<base::string16> found_uws_strings;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValues(kFoundUwsValueName, &found_uws_strings) !=
            ERROR_SUCCESS) {
      return;
    }

    rappor::RapporServiceImpl* rappor_service = nullptr;
    if (use_rappor)
      rappor_service = g_browser_process->rappor_service();

    bool parse_error = false;
    for (const base::string16& uws_string : found_uws_strings) {
      // All UwS ids are expected to be integers.
      uint32_t uws_id = 0;
      if (base::StringToUint(uws_string, &uws_id)) {
        RecordSparseHistogram(kFoundUwsMetricName, uws_id);
        if (rappor_service) {
          rappor_service->RecordSampleString(kFoundUwsMetricName,
                                             rappor::COARSE_RAPPOR_TYPE,
                                             base::UTF16ToUTF8(uws_string));
        }
      } else {
        parse_error = true;
      }
    }

    // Clean up the old value.
    reporter_key.DeleteValue(kFoundUwsValueName);
    RecordBooleanHistogram(kFoundUwsReadErrorMetricName, parse_error);
  }

  // Reports to UMA the memory usage of the software reporter tool as reported
  // by the tool itself in the Windows registry.
  void ReportMemoryUsage() const {
    base::win::RegKey reporter_key;
    DWORD memory_used = 0;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(kMemoryUsedValueName, &memory_used) !=
            ERROR_SUCCESS) {
      return;
    }
    RecordMemoryKBHistogram(kMemoryUsedMetricName, memory_used);
    reporter_key.DeleteValue(kMemoryUsedValueName);
  }

  // Reports the SwReporter run time with UMA both as reported by the tool via
  // the registry and as measured by |ReporterRunner|.
  void ReportRuntime(const base::TimeDelta& reporter_running_time) const {
    RecordLongTimesHistogram("SoftwareReporter.RunningTimeAccordingToChrome",
                             reporter_running_time);

    // TODO(b/641081): This should only have KEY_QUERY_VALUE and KEY_SET_VALUE.
    base::win::RegKey reporter_key;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_ALL_ACCESS) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kRunningTimeErrorMetricName,
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
      RecordLongTimesHistogram("SoftwareReporter.RunningTime",
                               registry_run_time);
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_NO_ERROR,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else if (!has_start_time && !has_end_time) {
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else if (!has_start_time) {
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else {
      DCHECK(!has_end_time);
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    }
  }

  // Reports the UwS scan times of the software reporter tool via UMA.
  void ReportScanTimes() const {
    base::string16 scan_times_key_path = base::StringPrintf(
        L"%ls\\%ls", registry_key_.c_str(), kScanTimesSubKey);
    // TODO(b/641081): This should only have KEY_QUERY_VALUE and KEY_SET_VALUE.
    base::win::RegKey scan_times_key;
    if (scan_times_key.Open(HKEY_CURRENT_USER, scan_times_key_path.c_str(),
                            KEY_ALL_ACCESS) != ERROR_SUCCESS) {
      return;
    }

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
        // We report the number of seconds plus one because it can take less
        // than one second to scan some UwS and the count passed to |AddCount|
        // must be at least one.
        RecordSparseHistogramCount(kScanTimesMetricName, uws_id,
                                   scan_time.InSeconds() + 1);
      }
    }
    // Clean up by deleting the scan times key, which is a subkey of the main
    // reporter key.
    scan_times_key.Close();
    base::win::RegKey reporter_key;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_ENUMERATE_SUB_KEYS) == ERROR_SUCCESS) {
      reporter_key.DeleteKey(kScanTimesSubKey);
    }
  }

  void RecordReporterStep(SwReporterUmaValue value) {
    RecordEnumerationHistogram(kStepMetricName, value, SW_REPORTER_MAX);
  }

  void RecordLogsUploadEnabled(SwReporterLogsUploadsEnabled value) {
    RecordEnumerationHistogram(kLogsUploadEnabledMetricName, value,
                               REPORTER_LOGS_UPLOADS_MAX);
  }

  void RecordLogsUploadResult() {
    base::win::RegKey reporter_key;
    DWORD logs_upload_result = 0;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_REGISTRY_KEY_INVALID,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    if (reporter_key.ReadValueDW(kLogsUploadResultValueName,
                                 &logs_upload_result) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_NOT_FOUND,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    if (logs_upload_result >= kSwReporterLogsUploadResultMax) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_OUT_OF_BOUNDS,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    RecordEnumerationHistogram(kLogsUploadResultMetricName,
                               static_cast<Sample>(logs_upload_result),
                               kSwReporterLogsUploadResultMax);
    reporter_key.DeleteValue(kLogsUploadResultValueName);
    RecordEnumerationHistogram(kLogsUploadResultRegistryErrorMetricName,
                               REPORTER_LOGS_UPLOAD_RESULT_ERROR_NO_ERROR,
                               REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
  }

 private:
  using Sample = base::HistogramBase::Sample;

  static constexpr base::HistogramBase::Flags kUmaHistogramFlag =
      base::HistogramBase::kUmaTargetedHistogramFlag;

  // Helper functions to record histograms with an optional suffix added to the
  // histogram name. The UMA_HISTOGRAM macros can't be used because they
  // require a constant string.

  std::string FullName(const std::string& name) const {
    if (suffix_.empty())
      return name;
    return base::StringPrintf("%s_%s", name.c_str(), suffix_.c_str());
  }

  void RecordBooleanHistogram(const std::string& name, bool sample) const {
    auto* histogram =
        base::BooleanHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->AddBoolean(sample);
  }

  void RecordEnumerationHistogram(const std::string& name,
                                  Sample sample,
                                  Sample boundary) const {
    // See HISTOGRAM_ENUMERATION_WITH_FLAG for the parameters to |FactoryGet|.
    auto* histogram = base::LinearHistogram::FactoryGet(
        FullName(name), 1, boundary, boundary + 1, kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordLongTimesHistogram(const std::string& name,
                                const base::TimeDelta& sample) const {
    // See UMA_HISTOGRAM_LONG_TIMES for the parameters to |FactoryTimeGet|.
    auto* histogram = base::Histogram::FactoryTimeGet(
        FullName(name), base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(1), 100, kUmaHistogramFlag);
    if (histogram)
      histogram->AddTime(sample);
  }

  void RecordMemoryKBHistogram(const std::string& name, Sample sample) const {
    // See UMA_HISTOGRAM_MEMORY_KB for the parameters to |FactoryGet|.
    auto* histogram = base::Histogram::FactoryGet(FullName(name), 1000, 500000,
                                                  50, kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordSparseHistogram(const std::string& name, Sample sample) const {
    auto* histogram =
        base::SparseHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordSparseHistogramCount(const std::string& name,
                                  Sample sample,
                                  int count) const {
    auto* histogram =
        base::SparseHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->AddCount(sample, count);
  }

  const std::string suffix_;
  const std::wstring registry_key_;
};

// Records the reporter step without a suffix. (For steps that are never run by
// the experimental reporter.)
void RecordReporterStepHistogram(SwReporterUmaValue value) {
  UMAHistogramReporter uma;
  uma.RecordReporterStep(value);
}

void DisplaySRTPrompt(const base::FilePath& download_path) {
  // Find the last active browser, which may be NULL, in which case we won't
  // show the prompt this time and will wait until the next run of the
  // reporter. We can't use other ways of finding a browser because we don't
  // have a profile.
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  Profile* profile = browser->profile();
  DCHECK(profile);

  // Make sure we have a tabbed browser since we need to anchor the bubble to
  // the toolbar's wrench menu. Create one if none exist already.
  if (browser->type() != Browser::TYPE_TABBED) {
    browser = chrome::FindTabbedBrowser(profile, false);
    if (!browser)
      browser = new Browser(Browser::CreateParams(profile, false));
  }
  GlobalErrorService* global_error_service =
      GlobalErrorServiceFactory::GetForProfile(profile);
  SRTGlobalError* global_error =
      new SRTGlobalError(global_error_service, download_path);

  // Ownership of |global_error| is passed to the service. The error removes
  // itself from the service and self-destructs when done.
  global_error_service->AddGlobalError(base::WrapUnique(global_error));

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
int LaunchAndWaitForExit(const SwReporterInvocation& invocation) {
  if (g_testing_delegate_)
    return g_testing_delegate_->LaunchReporter(invocation);
  base::Process reporter_process =
      base::LaunchProcess(invocation.command_line, base::LaunchOptions());
  // This exit code is used to identify that a reporter run didn't happen, so
  // the result should be ignored and a rerun scheduled for the usual delay.
  int exit_code = kReporterFailureExitCode;
  UMAHistogramReporter uma(invocation.suffix);
  if (reporter_process.IsValid()) {
    uma.RecordReporterStep(SW_REPORTER_START_EXECUTION);
    bool success = reporter_process.WaitForExit(&exit_code);
    DCHECK(success);
  } else {
    uma.RecordReporterStep(SW_REPORTER_FAILED_TO_START);
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
    data_use_measurement::DataUseUserData::AttachToFetcher(
        url_fetcher_.get(),
        data_use_measurement::DataUseUserData::SAFE_BROWSING);
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetMaxRetriesOn5xx(3);
    url_fetcher_->SaveResponseToTemporaryFile(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    // Adds the UMA bit to the download request if the user is enrolled in UMA.
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(
        profile_->GetResourceContext());
    net::HttpRequestHeaders headers;
    // Note: It's OK to pass |is_signed_in| false if it's unknown, as it does
    // not affect transmission of experiments coming from the variations server.
    bool is_signed_in = false;
    variations::AppendVariationHeaders(
        url_fetcher_->GetOriginalURL(), io_data->IsOffTheRecord(),
        ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(),
        is_signed_in, &headers);
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
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(SRTFetcher);
};

namespace {

// Try to fetch the SRT, and on success, show the prompt to run it.
void MaybeFetchSRT(Browser* browser, const base::Version& reporter_version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (g_testing_delegate_) {
    g_testing_delegate_->TriggerPrompt(browser, reporter_version.GetString());
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
  prefs->SetString(prefs::kSwReporterPromptVersion,
                   reporter_version.GetString());

  // Download the SRT.
  RecordReporterStepHistogram(SW_REPORTER_DOWNLOAD_START);

  // All the work happens in the self-deleting class below.
  new SRTFetcher(profile);
}

base::Time Now() {
  return g_testing_delegate_ ? g_testing_delegate_->Now() : base::Time::Now();
}

}  // namespace

// This class tries to run a queue of reporters and react to their exit codes.
// It schedules subsequent runs of the queue as needed, or retries as soon as a
// browser is available when none is on first try.
class ReporterRunner : public chrome::BrowserListObserver {
 public:
  // Registers |invocations| to run next time |TryToRun| is scheduled. (And if
  // it's not already scheduled, call it now.)
  static void ScheduleInvocations(const SwReporterQueue& invocations,
                                  const base::Version& version) {
    if (!instance_) {
      instance_ = new ReporterRunner;
      ANNOTATE_LEAKING_OBJECT_PTR(instance_);
    }
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // There's nothing to do if the invocation parameters and version of the
    // reporter have not changed, we just keep running the tasks that are
    // running now.
    if (instance_->pending_invocations_ == invocations &&
        instance_->version_.IsValid() && instance_->version_ == version)
      return;

    instance_->pending_invocations_ = invocations;
    instance_->version_ = version;
    if (instance_->first_run_) {
      instance_->first_run_ = false;
      instance_->TryToRun();
    }
  }

 private:
  ReporterRunner() {}
  ~ReporterRunner() override {}

  // BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override {}
  void OnBrowserRemoved(Browser* browser) override {}
  void OnBrowserAdded(Browser* browser) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(browser);
    MaybeFetchSRT(browser, version_);
    BrowserList::RemoveObserver(this);
  }

  // Launches the command line at the head of the queue.
  void ScheduleNextInvocation() {
    DCHECK(!current_invocations_.empty());
    auto next_invocation = current_invocations_.front();
    current_invocations_.pop();

    AppendInvocationSpecificSwitches(&next_invocation);

    base::TaskRunner* task_runner =
        g_testing_delegate_ ? g_testing_delegate_->BlockingTaskRunner()
                            : blocking_task_runner_.get();
    base::PostTaskAndReplyWithResult(
        task_runner, FROM_HERE,
        base::Bind(&LaunchAndWaitForExit, next_invocation),
        base::Bind(&ReporterRunner::ReporterDone, base::Unretained(this), Now(),
                   version_, next_invocation));
  }

  // This method is called on the UI thread when an invocation of the reporter
  // has completed. This is run as a task posted from an interruptible worker
  // thread so should be resilient to unexpected shutdown.
  void ReporterDone(const base::Time& reporter_start_time,
                    const base::Version& version,
                    const SwReporterInvocation& finished_invocation,
                    int exit_code) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    base::Time now = Now();
    base::TimeDelta reporter_running_time = now - reporter_start_time;

    // Don't continue the current queue of reporters if one failed to launch.
    if (exit_code == kReporterFailureExitCode)
      current_invocations_ = SwReporterQueue();

    // As soon as we're not running this queue, schedule the next overall queue
    // run after the regular delay. (If there was a failure it's not worth
    // retrying earlier, risking running too often if it always fails, since
    // not many users fail here.)
    if (current_invocations_.empty()) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ReporterRunner::TryToRun, base::Unretained(this)),
          base::TimeDelta::FromDays(days_between_reporter_runs_));
    } else {
      ScheduleNextInvocation();
    }

    // If the reporter failed to launch, do not process the results. (The exit
    // code itself doesn't need to be logged in this case because
    // SW_REPORTER_FAILED_TO_START is logged in |LaunchAndWaitForExit|.)
    if (exit_code == kReporterFailureExitCode)
      return;

    UMAHistogramReporter uma(finished_invocation.suffix);
    uma.ReportVersion(version);
    uma.ReportExitCode(exit_code);
    uma.ReportEngineErrorCode();
    uma.ReportFoundUwS(finished_invocation.BehaviourIsSupported(
        SwReporterInvocation::BEHAVIOUR_LOG_TO_RAPPOR));

    PrefService* local_state = g_browser_process->local_state();
    if (local_state) {
      if (finished_invocation.BehaviourIsSupported(
              SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS)) {
        local_state->SetInteger(prefs::kSwReporterLastExitCode, exit_code);
      }
      local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                            now.ToInternalValue());
    }
    uma.ReportRuntime(reporter_running_time);
    uma.ReportScanTimes();
    uma.ReportMemoryUsage();
    if (finished_invocation.logs_upload_enabled)
      uma.RecordLogsUploadResult();

    if (!finished_invocation.BehaviourIsSupported(
            SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT)) {
      return;
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
    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      RecordReporterStepHistogram(SW_REPORTER_NO_BROWSER);
      BrowserList::AddObserver(this);
    } else {
      MaybeFetchSRT(browser, version_);
    }
  }

  void TryToRun() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    PrefService* local_state = g_browser_process->local_state();

    if (!version_.IsValid() || !local_state) {
      // TODO(b/641081): This doesn't look right. Even on first run, |version_|
      // should be valid (and this is already checked in RunSwReporters). We
      // should abort if local_state is missing, but this has nothing to do
      // with |first_run_|.
      DCHECK(first_run_);
      return;
    }

    // Run a queue of reporters if none have been triggered in the last
    // |days_between_reporter_runs_| days, which depends if there is a pending
    // prompt to be added to Chrome's menu.
    if (local_state->GetBoolean(prefs::kSwReporterPendingPrompt)) {
      days_between_reporter_runs_ = kDaysBetweenSwReporterRunsForPendingPrompt;
      RecordReporterStepHistogram(SW_REPORTER_RAN_DAILY);
    } else {
      days_between_reporter_runs_ = kDaysBetweenSuccessfulSwReporterRuns;
    }
    const base::Time now = Now();
    const base::Time last_time_triggered = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kSwReporterLastTimeTriggered));
    const base::Time next_trigger(
        last_time_triggered +
        base::TimeDelta::FromDays(days_between_reporter_runs_));
    if (!pending_invocations_.empty() &&
        (next_trigger <= now ||
         // Also make sure the kSwReporterLastTimeTriggered value is not set in
         // the future.
         last_time_triggered > now)) {
      const base::Time last_time_sent_logs = base::Time::FromInternalValue(
          local_state->GetInt64(prefs::kSwReporterLastTimeSentReport));
      const base::Time next_time_send_logs =
          last_time_sent_logs +
          base::TimeDelta::FromDays(kDaysBetweenReporterLogsSent);
      // Send the logs for this whole queue of invocations if the last send is
      // in the future or if logs have been sent at least
      // |kSwReporterLastTimeSentReport| days ago. The former is intended as a
      // measure for failure recovery, in case the time in local state is
      // incorrectly set to the future.
      in_logs_upload_period_ =
          last_time_sent_logs > now || next_time_send_logs <= now;

      DCHECK(current_invocations_.empty());
      current_invocations_ = pending_invocations_;
      ScheduleNextInvocation();
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ReporterRunner::TryToRun, base::Unretained(this)),
          next_trigger - now);
    }
  }

  // Returns true if the experiment to send reporter logs is enabled, the user
  // opted into Safe Browsing extended reporting, and this queue of invocations
  // started during the logs upload interval.
  bool ShouldSendReporterLogs(const std::string& suffix,
                              const PrefService& local_state) {
    UMAHistogramReporter uma(suffix);
    if (!SafeBrowsingExtendedReportingEnabled()) {
      uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_SBER_DISABLED);
      return false;
    }
    if (!in_logs_upload_period_) {
      uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_RECENTLY_SENT_LOGS);
      return false;
    }
    uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_ENABLED);
    return true;
  }

  // Appends switches to the next invocation that depend on the user current
  // state with respect to opting into extended Safe Browsing reporting and
  // metrics and crash reporting. The invocation object is changed locally right
  // before the actual process is launched because user status can change
  // between this and the next run for this ReporterRunner object. For example,
  // the ReporterDone() callback schedules the next run for a few days later,
  // and the user might have changed settings in the meantime.
  void AppendInvocationSpecificSwitches(SwReporterInvocation* next_invocation) {
    // Add switches for users who opted into extended Safe Browsing reporting.
    PrefService* local_state = g_browser_process->local_state();
    if (next_invocation->BehaviourIsSupported(
            SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS) &&
        local_state &&
        ShouldSendReporterLogs(next_invocation->suffix, *local_state)) {
      next_invocation->logs_upload_enabled = true;
      AddSwitchesForExtendedReportingUser(next_invocation);
      // Set the local state value before the first attempt to run the
      // reporter, because we only want to upload logs once in the window
      // defined by |kDaysBetweenReporterLogsSent|. If we set with other local
      // state values after the reporter runs, we could send logs again too
      // quickly (for example, if Chrome stops before the reporter finishes).
      local_state->SetInt64(prefs::kSwReporterLastTimeSentReport,
                            Now().ToInternalValue());
    }

    if (ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled())
      next_invocation->command_line.AppendSwitch(kEnableCrashReporting);
  }

  // Adds switches to be sent to the Software Reporter when the user opted into
  // extended Safe Browsing reporting and is not incognito.
  void AddSwitchesForExtendedReportingUser(SwReporterInvocation* invocation) {
    invocation->command_line.AppendSwitch(kExtendedSafeBrowsingEnabledSwitch);
    invocation->command_line.AppendSwitchASCII(
        kChromeVersionSwitch, version_info::GetVersionNumber());
    invocation->command_line.AppendSwitchNative(
        kChromeChannelSwitch, base::IntToString16(ChannelAsInt()));
  }

  bool first_run_ = true;

  // The queue of invocations that are currently running.
  SwReporterQueue current_invocations_;

  // The invocations to run next time the SwReporter is run.
  SwReporterQueue pending_invocations_;

  base::Version version_;

  scoped_refptr<base::TaskRunner> blocking_task_runner_ =
      base::CreateTaskRunnerWithTraits(
          // LaunchAndWaitForExit() creates (MayBlock()) and joins
          // (WithBaseSyncPrimitives()) a process.
          base::TaskTraits()
              .WithShutdownBehavior(
                  base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
              .WithPriority(base::TaskPriority::BACKGROUND)
              .MayBlock()
              .WithBaseSyncPrimitives());

  // This value is used to identify how long to wait before starting a new run
  // of the reporter queue. It's initialized with the default value and may be
  // changed to a different value when a prompt is pending and the reporter
  // should be run before adding the global error to the Chrome menu.
  int days_between_reporter_runs_ = kDaysBetweenSuccessfulSwReporterRuns;

  // This will be true if the current queue of invocations started at a time
  // when logs should be uploaded.
  bool in_logs_upload_period_ = false;

  // A single leaky instance.
  static ReporterRunner* instance_;

  DISALLOW_COPY_AND_ASSIGN(ReporterRunner);
};

ReporterRunner* ReporterRunner::instance_ = nullptr;

SwReporterInvocation::SwReporterInvocation()
    : command_line(base::CommandLine::NO_PROGRAM) {}

SwReporterInvocation SwReporterInvocation::FromFilePath(
    const base::FilePath& exe_path) {
  SwReporterInvocation invocation;
  invocation.command_line = base::CommandLine(exe_path);
  return invocation;
}

SwReporterInvocation SwReporterInvocation::FromCommandLine(
    const base::CommandLine& command_line) {
  SwReporterInvocation invocation;
  invocation.command_line = command_line;
  return invocation;
}

bool SwReporterInvocation::operator==(const SwReporterInvocation& other) const {
  return command_line.argv() == other.command_line.argv() &&
         suffix == other.suffix &&
         supported_behaviours == other.supported_behaviours &&
         logs_upload_enabled == other.logs_upload_enabled;
}

bool SwReporterInvocation::BehaviourIsSupported(
    SwReporterInvocation::Behaviours intended_behaviour) const {
  return (supported_behaviours & intended_behaviour) != 0;
}

void RunSwReporters(const SwReporterQueue& invocations,
                    const base::Version& version) {
  DCHECK(!invocations.empty());
  DCHECK(version.IsValid());
  ReporterRunner::ScheduleInvocations(invocations, version);
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

  base::win::RegKey srt_cleaner_key;
  return srt_cleaner_key.Open(HKEY_CURRENT_USER, cleaner_key_path.c_str(),
                              KEY_QUERY_VALUE) == ERROR_SUCCESS &&
         srt_cleaner_key.GetValueCount() > 0;
}

void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate) {
  g_testing_delegate_ = delegate;
}

}  // namespace safe_browsing
