// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_tokenizer.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "chrome/browser/safe_browsing/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using safe_browsing::IsInSRTPromptFieldTrialGroups;

namespace component_updater {

namespace {

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum SwReporterUmaValue {
  SW_REPORTER_EXPLICIT_REQUEST = 0,        // Deprecated.
  SW_REPORTER_STARTUP_RETRY = 1,           // Deprecated.
  SW_REPORTER_RETRIED_TOO_MANY_TIMES = 2,  // Deprecated.
  SW_REPORTER_START_EXECUTION = 3,
  SW_REPORTER_FAILED_TO_START = 4,
  SW_REPORTER_REGISTRY_EXIT_CODE = 5,
  SW_REPORTER_RESET_RETRIES = 6,  // Deprecated.
  SW_REPORTER_DOWNLOAD_START = 7,
  SW_REPORTER_MAX,
};

// The maximum number of times to retry a download on startup.
const int kMaxRetry = 20;

// The number of days to wait before triggering another sw reporter run.
const int kDaysBetweenSwReporterRuns = 7;

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8_t kSha256Hash[] = {0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6,
                               0xf4, 0xc9, 0x78, 0x6c, 0x0c, 0x24, 0x73, 0x3e,
                               0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7, 0xb7, 0x1c,
                               0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

// Where to fetch the reporter exit code in the registry.
const wchar_t kSoftwareRemovalToolRegistryKey[] =
    L"Software\\Google\\Software Removal Tool";
const wchar_t kCleanerSuffixRegistryKey[] = L"Cleaner";
const wchar_t kEndTimeRegistryValueName[] = L"EndTime";
const wchar_t kExitCodeRegistryValueName[] = L"ExitCode";
const wchar_t kStartTimeRegistryValueName[] = L"StartTime";
const wchar_t kUploadResultsValueName[] = L"UploadResults";
const wchar_t kVersionRegistryValueName[] = L"Version";

// Exit codes that identify that a cleanup is needed.
const int kCleanupNeeded = 0;
const int kNothingFound = 2;
const int kPostRebootCleanupNeeded = 4;
const int kDelayedPostRebootCleanupNeeded = 15;

void ReportUmaStep(SwReporterUmaValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Step", value, SW_REPORTER_MAX);
}

void ReportVersionWithUma(const base::Version& version) {
  DCHECK(!version.components().empty());
  // The minor version is the 2nd last component of the version,
  // or just the first component if there is only 1.
  uint32_t minor_version = 0;
  if (version.components().size() > 1)
    minor_version = version.components()[version.components().size() - 2];
  else
    minor_version = version.components()[0];
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MinorVersion", minor_version);

  // The major version for X.Y.Z is X*256^3+Y*256+Z. If there are additional
  // components, only the first three count, and if there are less than 3, the
  // missing values are just replaced by zero. So 1 is equivalent 1.0.0.
  DCHECK_LT(version.components()[0], 0x100);
  uint32_t major_version = 0x1000000 * version.components()[0];
  if (version.components().size() >= 2) {
    DCHECK_LT(version.components()[1], 0x10000);
    major_version += 0x100 * version.components()[1];
  }
  if (version.components().size() >= 3) {
    DCHECK_LT(version.components()[2], 0x100);
    major_version += version.components()[2];
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MajorVersion", major_version);
}

void ReportUploadsWithUma(const base::string16& upload_results) {
  base::WStringTokenizer tokenizer(upload_results, L";");
  int failure_count = 0;
  int success_count = 0;
  int longest_failure_run = 0;
  int current_failure_run = 0;
  bool last_result = false;
  while (tokenizer.GetNext()) {
    if (tokenizer.token() == L"0") {
      ++failure_count;
      ++current_failure_run;
      last_result = false;
    } else {
      ++success_count;
      current_failure_run = 0;
      last_result = true;
    }

    if (current_failure_run > longest_failure_run)
      longest_failure_run = current_failure_run;
  }

  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadFailureCount",
                           failure_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadSuccessCount",
                           success_count);
  UMA_HISTOGRAM_COUNTS_100("SoftwareReporter.UploadLongestFailureRun",
                           longest_failure_run);
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.LastUploadResult", last_result);
}

// This function is called on the UI thread to report the SwReporter exit code
// and then clear it from the registry as well as clear the execution state
// from the local state. This could be called from an interruptible worker
// thread so should be resilient to unexpected shutdown. |version| is provided
// so the kSwReporterPromptVersion prefs can be set.
void ReportAndClearExitCode(int exit_code, const std::string& version) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.ExitCode", exit_code);

  if (g_browser_process && g_browser_process->local_state()) {
    g_browser_process->local_state()->SetInteger(prefs::kSwReporterLastExitCode,
                                                 exit_code);
  }

  base::win::RegKey srt_key(HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey,
                            KEY_SET_VALUE);
  srt_key.DeleteValue(kExitCodeRegistryValueName);

  if ((exit_code != kPostRebootCleanupNeeded && exit_code != kCleanupNeeded) ||
      !IsInSRTPromptFieldTrialGroups()) {
    return;
  }

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

  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Don't show the prompt again if it's been shown before for this profile
  // and for the current Finch seed.
  std::string incoming_seed = safe_browsing::GetIncomingSRTSeed();
  std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
  if (!incoming_seed.empty() && incoming_seed == old_seed)
    return;

  if (!incoming_seed.empty())
    prefs->SetString(prefs::kSwReporterPromptSeed, incoming_seed);
  prefs->SetString(prefs::kSwReporterPromptVersion, version);
  prefs->SetInteger(prefs::kSwReporterPromptReason, exit_code);

  // Download the SRT.
  ReportUmaStep(SW_REPORTER_DOWNLOAD_START);
  safe_browsing::FetchSRTAndDisplayBubble();
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be interrupted
// by a shutdown at anytime, so it shouldn't depend on anything external that
// could be shutdown beforehand.
void LaunchAndWaitForExit(const base::FilePath& exe_path,
                          const std::string& version) {
  const base::CommandLine reporter_command_line(exe_path);
  base::Process scan_reporter_process =
      base::LaunchProcess(reporter_command_line, base::LaunchOptions());
  if (!scan_reporter_process.IsValid()) {
    ReportUmaStep(SW_REPORTER_FAILED_TO_START);
    return;
  }
  ReportUmaStep(SW_REPORTER_START_EXECUTION);

  int exit_code = -1;
  bool success = scan_reporter_process.WaitForExit(&exit_code);
  DCHECK(success);
  // It's OK if this doesn't complete, the work will continue on next startup.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ReportAndClearExitCode, exit_code, version));
}

class SwReporterInstallerTraits : public ComponentInstallerTraits {
 public:
  explicit SwReporterInstallerTraits(PrefService* prefs) : prefs_(prefs) {}

  virtual ~SwReporterInstallerTraits() {}

  virtual bool VerifyInstallation(const base::DictionaryValue& manifest,
                                  const base::FilePath& dir) const {
    return base::PathExists(dir.Append(kSwReporterExeName));
  }

  virtual bool CanAutoUpdate() const { return true; }

  virtual bool OnCustomInstall(const base::DictionaryValue& manifest,
                               const base::FilePath& install_dir) {
    return true;
  }

  virtual void ComponentReady(const base::Version& version,
                              const base::FilePath& install_dir,
                              scoped_ptr<base::DictionaryValue> manifest) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    ReportVersionWithUma(version);

    wcsncpy_s(version_dir_,
              _MAX_PATH,
              install_dir.value().c_str(),
              install_dir.value().size());

    // A previous run may have results in the registry, so check and report
    // them if present.
    std::string version_string(version.GetString());
    base::win::RegKey srt_key(
        HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey, KEY_READ);
    DWORD exit_code;
    if (srt_key.Valid() &&
        srt_key.ReadValueDW(kExitCodeRegistryValueName, &exit_code) ==
            ERROR_SUCCESS) {
      ReportUmaStep(SW_REPORTER_REGISTRY_EXIT_CODE);
      ReportAndClearExitCode(exit_code, version_string);
    }

    // If we can't access local state, we can't see when we last ran, so
    // just exit without running.
    if (!g_browser_process || !g_browser_process->local_state())
      return;

    // Run the reporter if it hasn't been triggered in the
    // kDaysBetweenSwReporterRuns days.
    const base::Time last_time_triggered = base::Time::FromInternalValue(
        g_browser_process->local_state()->GetInt64(
            prefs::kSwReporterLastTimeTriggered));
    if ((base::Time::Now() - last_time_triggered).InDays() >=
        kDaysBetweenSwReporterRuns) {
      g_browser_process->local_state()->SetInt64(
          prefs::kSwReporterLastTimeTriggered,
          base::Time::Now().ToInternalValue());

      base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(&LaunchAndWaitForExit,
                     install_dir.Append(kSwReporterExeName),
                     version_string),
          true);
    }
  }

  virtual base::FilePath GetBaseDirectory() const { return install_dir(); }

  virtual void GetHash(std::vector<uint8_t>* hash) const { GetPkHash(hash); }

  virtual std::string GetName() const { return "Software Reporter Tool"; }

  static base::FilePath install_dir() {
    // The base directory on windows looks like:
    // <profile>\AppData\Local\Google\Chrome\User Data\SwReporter\.
    base::FilePath result;
    PathService::Get(DIR_SW_REPORTER, &result);
    return result;
  }

  static std::string ID() {
    update_client::CrxComponent component;
    component.version = Version("0.0.0.0");
    GetPkHash(&component.pk_hash);
    return update_client::GetCrxComponentID(component);
  }

  static base::FilePath VersionPath() { return base::FilePath(version_dir_); }

 private:
  static void GetPkHash(std::vector<uint8_t>* hash) {
    DCHECK(hash);
    hash->assign(kSha256Hash, kSha256Hash + sizeof(kSha256Hash));
  }

  PrefService* prefs_;
  static wchar_t version_dir_[_MAX_PATH];
};

wchar_t SwReporterInstallerTraits::version_dir_[] = {};

}  // namespace

void RegisterSwReporterComponent(ComponentUpdateService* cus,
                                 PrefService* prefs) {
  // The Sw reporter doesn't need to run if the user isn't reporting metrics and
  // isn't in the SRTPrompt field trial "On" group.
  if (!ChromeMetricsServiceAccessor::IsMetricsReportingEnabled() &&
      !IsInSRTPromptFieldTrialGroups()) {
    return;
  }

  // Check if we have information from Cleaner and record UMA statistics.
  base::string16 cleaner_key_name(kSoftwareRemovalToolRegistryKey);
  cleaner_key_name.append(1, L'\\').append(kCleanerSuffixRegistryKey);
  base::win::RegKey cleaner_key(
      HKEY_CURRENT_USER, cleaner_key_name.c_str(), KEY_ALL_ACCESS);
  // Cleaner is assumed to have run if we have a start time.
  if (cleaner_key.Valid() &&
      cleaner_key.HasValue(kStartTimeRegistryValueName)) {
    // Get version number.
    if (cleaner_key.HasValue(kVersionRegistryValueName)) {
      DWORD version;
      cleaner_key.ReadValueDW(kVersionRegistryValueName, &version);
      UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.Version", version);
      cleaner_key.DeleteValue(kVersionRegistryValueName);
    }
    // Get start & end time. If we don't have an end time, we can assume the
    // cleaner has not completed.
    int64 start_time_value;
    cleaner_key.ReadInt64(kStartTimeRegistryValueName, &start_time_value);

    bool completed = cleaner_key.HasValue(kEndTimeRegistryValueName);
    UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.Cleaner.HasCompleted", completed);
    if (completed) {
      int64 end_time_value;
      cleaner_key.ReadInt64(kEndTimeRegistryValueName, &end_time_value);
      cleaner_key.DeleteValue(kEndTimeRegistryValueName);
      base::TimeDelta run_time(base::Time::FromInternalValue(end_time_value) -
          base::Time::FromInternalValue(start_time_value));
      UMA_HISTOGRAM_LONG_TIMES("SoftwareReporter.Cleaner.RunningTime",
          run_time);
    }
    // Get exit code. Assume nothing was found if we can't read the exit code.
    DWORD exit_code = kNothingFound;
    if (cleaner_key.HasValue(kExitCodeRegistryValueName)) {
      cleaner_key.ReadValueDW(kExitCodeRegistryValueName, &exit_code);
      UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.Cleaner.ExitCode",
          exit_code);
      cleaner_key.DeleteValue(kExitCodeRegistryValueName);
    }
    cleaner_key.DeleteValue(kStartTimeRegistryValueName);

    if (exit_code == kPostRebootCleanupNeeded ||
        exit_code == kDelayedPostRebootCleanupNeeded) {
      // Check if we are running after the user has rebooted.
      base::TimeDelta elapsed(base::Time::Now() -
                              base::Time::FromInternalValue(start_time_value));
      DCHECK_GT(elapsed.InMilliseconds(), 0);
      UMA_HISTOGRAM_BOOLEAN(
          "SoftwareReporter.Cleaner.HasRebooted",
          static_cast<uint64>(elapsed.InMilliseconds()) > ::GetTickCount());
    }

    if (cleaner_key.HasValue(kUploadResultsValueName)) {
      base::string16 upload_results;
      cleaner_key.ReadValue(kUploadResultsValueName, &upload_results);
      ReportUploadsWithUma(upload_results);
    }
  }

  // Install the component.
  scoped_ptr<ComponentInstallerTraits> traits(
      new SwReporterInstallerTraits(prefs));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus);
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSwReporterLastTimeTriggered, 0);
  registry->RegisterIntegerPref(prefs::kSwReporterLastExitCode, -1);
}

void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kSwReporterPromptReason, -1,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kSwReporterPromptVersion, "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kSwReporterPromptSeed, "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace component_updater
