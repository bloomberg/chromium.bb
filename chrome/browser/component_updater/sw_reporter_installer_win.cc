// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <stdint.h>
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
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/srt_global_error_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/component_updater/default_component_installer.h"
#include "components/component_updater/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

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
const wchar_t kExitCodeRegistryValueName[] = L"ExitCode";

// Field trial strings.
const char kSRTPromptTrialName[] = "SRTPromptFieldTrial";
const char kSRTPromptOnGroup[] = "On";

// Exit codes that identify that a cleanup is needed.
const int kCleanupNeeded = 0;
const int kPostRebootCleanupNeeded = 4;

void ReportUmaStep(SwReporterUmaValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Step", value, SW_REPORTER_MAX);
}

void ReportUmaVersion(const base::Version& version) {
  DCHECK(!version.components().empty());
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MinorVersion",
                              version.components().back());
  // The major version uses the 1st component value (when there is more than
  // one, since the last one is always the minor version) as a hi word in a
  // double word. The low word is either the second component (when there are
  // only three) or the 3rd one if there are at least 4. E.g., for W.X.Y.Z, we
  // ignore X, and Z is the minor version. We compute the major version with W
  // as the hi word, and Y as the low word. For X.Y.Z, we use X and Y as hi and
  // low words, and if we would have Y.Z we would use Y as the hi word and 0 as
  // the low word. major version is 0 if the version only has one component.
  uint32_t major_version = 0;
  if (version.components().size() > 1)
    major_version = 0x10000 * version.components()[0];
  if (version.components().size() < 4 && version.components().size() > 2)
    major_version += version.components()[1];
  else if (version.components().size() > 3)
    major_version += version.components()[2];
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.MajorVersion", major_version);
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

  if ((exit_code == kPostRebootCleanupNeeded || exit_code == kCleanupNeeded) &&
      base::FieldTrialList::FindFullName(kSRTPromptTrialName) ==
          kSRTPromptOnGroup) {
    // Find the last active browser, which may be NULL, in which case we won't
    // show the prompt this time and will wait until the next run of the
    // reporter. We can't use other ways of finding a browser because we don't
    // have a profile.
    chrome::HostDesktopType desktop_type = chrome::GetActiveDesktop();
    Browser* browser = chrome::FindLastActiveWithHostDesktopType(desktop_type);
    if (browser) {
      Profile* profile = browser->profile();
      // Don't show the prompt again if it's been shown before for this profile.
      DCHECK(profile);
      const std::string prompt_version =
          profile->GetPrefs()->GetString(prefs::kSwReporterPromptVersion);
      if (prompt_version.empty()) {
        profile->GetPrefs()->SetString(prefs::kSwReporterPromptVersion,
                                       version);
        profile->GetPrefs()->SetInteger(prefs::kSwReporterPromptReason,
                                        exit_code);
        // Now that we have a profile, make sure we have a tabbed browser since
        // we need to anchor the bubble to the toolbar's wrench menu. Create one
        // if none exist already.
        if (browser->type() != Browser::TYPE_TABBED) {
          browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
          if (!browser)
            browser = new Browser(Browser::CreateParams(profile, desktop_type));
        }
        GlobalErrorService* global_error_service =
            GlobalErrorServiceFactory::GetForProfile(profile);
        SRTGlobalError* global_error = new SRTGlobalError(global_error_service);
        // |global_error_service| takes ownership of |global_error| and keeps it
        // alive until RemoveGlobalError() is called, and even then, the object
        // is not destroyed, the caller of RemoveGlobalError is responsible to
        // destroy it, and in the case of the SRTGlobalError, it deletes itself
        // but only after the bubble has been interacted with.
        global_error_service->AddGlobalError(global_error);

        // Do not try to show bubble if another GlobalError is already showing
        // one. The bubble will be shown once the others have been dismissed.
        const GlobalErrorService::GlobalErrorList& global_errors(
            global_error_service->errors());
        GlobalErrorService::GlobalErrorList::const_iterator it;
        for (it = global_errors.begin(); it != global_errors.end(); ++it) {
          if ((*it)->GetBubbleView())
            break;
        }
        if (it == global_errors.end())
          global_error->ShowBubbleView(browser);
      }
    }
  }

  base::win::RegKey srt_key(
      HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey, KEY_WRITE);
  srt_key.DeleteValue(kExitCodeRegistryValueName);
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be interrupted
// by a shutdown at anytime, so it shouldn't depend on anything external that
// could be shutdown beforehand.
void LaunchAndWaitForExit(const base::FilePath& exe_path,
                          const std::string& version) {
  const base::CommandLine reporter_command_line(exe_path);
  base::ProcessHandle scan_reporter_process = base::kNullProcessHandle;
  if (!base::LaunchProcess(reporter_command_line,
                           base::LaunchOptions(),
                           &scan_reporter_process)) {
    ReportUmaStep(SW_REPORTER_FAILED_TO_START);
    return;
  }
  ReportUmaStep(SW_REPORTER_START_EXECUTION);

  int exit_code = -1;
  bool success = base::WaitForExitCode(scan_reporter_process, &exit_code);
  DCHECK(success);
  base::CloseProcessHandle(scan_reporter_process);
  scan_reporter_process = base::kNullProcessHandle;
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

  virtual bool VerifyInstallation(const base::FilePath& dir) const {
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
    ReportUmaVersion(version);

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
    CrxComponent component;
    component.version = Version("0.0.0.0");
    GetPkHash(&component.pk_hash);
    return component_updater::GetCrxComponentID(component);
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
      base::FieldTrialList::FindFullName(kSRTPromptTrialName) !=
          kSRTPromptOnGroup) {
    return;
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
      prefs::kSwReporterPromptReason,
      -1,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kSwReporterPromptVersion,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace component_updater
