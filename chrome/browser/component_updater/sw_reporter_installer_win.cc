// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/component_updater/default_component_installer.h"
#include "components/component_updater/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum SwReporterUmaValue {
  SW_REPORTER_EXPLICIT_REQUEST = 0,
  SW_REPORTER_STARTUP_RETRY = 1,
  SW_REPORTER_RETRIED_TOO_MANY_TIMES = 2,
  SW_REPORTER_START_EXECUTION = 3,
  SW_REPORTER_FAILED_TO_START = 4,
  SW_REPORTER_REGISTRY_EXIT_CODE = 5,
  SW_REPORTER_RESET_RETRIES = 6,
  SW_REPORTER_MAX,
};

// The maximum number of times to retry a download on startup.
const int kMaxRetry = 20;

// CRX hash. The extension id is: gkmgaooipdjhmangpemjhigmamcehddo. The hash was
// generated in Python with something like this:
// hashlib.sha256().update(open("<file>.crx").read()[16:16+294]).digest().
const uint8 kSha256Hash[] = {0x6a, 0xc6, 0x0e, 0xe8, 0xf3, 0x97, 0xc0, 0xd6,
                             0xf4, 0xc9, 0x78, 0x6c, 0x0c, 0x24, 0x73, 0x3e,
                             0x05, 0xa5, 0x62, 0x4b, 0x2e, 0xc7, 0xb7, 0x1c,
                             0x5f, 0xea, 0xf0, 0x88, 0xf6, 0x97, 0x9b, 0xc7};

const base::FilePath::CharType kSwReporterExeName[] =
    FILE_PATH_LITERAL("software_reporter_tool.exe");

// Where to fetch the reporter exit code in the registry.
const wchar_t kSoftwareRemovalToolRegistryKey[] =
    L"Software\\Google\\Software Removal Tool";
const wchar_t kExitCodeRegistryValueName[] = L"ExitCode";

void ReportUmaStep(SwReporterUmaValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Step", value, SW_REPORTER_MAX);
}

// This function is called on the UI thread to report the SwReporter exit code
// and then clear it from the registry as well as clear the execution state
// from the local state. This could be called from an interruptible worker
// thread so should be resilient to unexpected shutdown.
void ReportAndClearExitCode(int exit_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("SoftwareReporter.ExitCode", exit_code);

  base::win::RegKey srt_key(
      HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey, KEY_WRITE);
  srt_key.DeleteValue(kExitCodeRegistryValueName);

  // Now that we are done we can reset the try count.
  g_browser_process->local_state()->SetInteger(
      prefs::kSwReporterExecuteTryCount, 0);
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be interrupted
// by a shutdown at anytime, so it shouldn't depend on anything external that
// could be shutdown beforehand.
void LaunchAndWaitForExit(const base::FilePath& exe_path) {
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
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ReportAndClearExitCode, exit_code));
}

void ExecuteReporter(const base::FilePath& install_dir) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&LaunchAndWaitForExit, install_dir.Append(kSwReporterExeName)),
      true);
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
    wcsncpy_s(version_dir_,
              _MAX_PATH,
              install_dir.value().c_str(),
              install_dir.value().size());
    // Only execute the reporter if there is still a pending request for it.
    if (prefs_->GetInteger(prefs::kSwReporterExecuteTryCount) > 0)
      ExecuteReporter(install_dir);
  }

  virtual base::FilePath GetBaseDirectory() const { return install_dir(); }

  virtual void GetHash(std::vector<uint8>* hash) const { GetPkHash(hash); }

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
  static void GetPkHash(std::vector<uint8>* hash) {
    DCHECK(hash);
    hash->assign(kSha256Hash, kSha256Hash + sizeof(kSha256Hash));
  }

  PrefService* prefs_;
  static wchar_t version_dir_[_MAX_PATH];
};

wchar_t SwReporterInstallerTraits::version_dir_[] = {};

void RegisterComponent(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ComponentInstallerTraits> traits(
      new SwReporterInstallerTraits(prefs));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus);
}

// We need a conditional version of register component so that it can be called
// back on the UI thread after validating on the File thread that the component
// path exists and we must re-register on startup for example.
void MaybeRegisterComponent(ComponentUpdateService* cus,
                            PrefService* prefs,
                            bool register_component) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (register_component)
    RegisterComponent(cus, prefs);
}

}  // namespace

void ExecuteSwReporter(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we have a pending execution, send metrics about it so we can account for
  // missing executions.
  if (prefs->GetInteger(prefs::kSwReporterExecuteTryCount) > 0)
    ReportUmaStep(SW_REPORTER_RESET_RETRIES);
  // This is an explicit call, so let's forget about previous incomplete
  // execution attempts and start from scratch.
  prefs->SetInteger(prefs::kSwReporterExecuteTryCount, kMaxRetry);
  ReportUmaStep(SW_REPORTER_EXPLICIT_REQUEST);
  const std::vector<std::string> registered_components(cus->GetComponentIDs());
  if (std::find(registered_components.begin(),
                registered_components.end(),
                SwReporterInstallerTraits::ID()) ==
      registered_components.end()) {
    RegisterComponent(cus, prefs);
  } else if (!SwReporterInstallerTraits::VersionPath().empty()) {
    // Here, we already have a fully registered and installed component
    // available for immediate use. This doesn't handle cases where the version
    // folder is there but the executable is not within in. This is a corruption
    // we don't want to handle here.
    ExecuteReporter(SwReporterInstallerTraits::VersionPath());
  }
  // If the component is registered but the version path is not available, it
  // means the component was not fully installed yet, and it should run the
  // reporter when ComponentReady is called.
}

void ExecutePendingSwReporter(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Register the existing component for updates.
  base::PostTaskAndReplyWithResult(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      FROM_HERE,
      base::Bind(&base::PathExists, SwReporterInstallerTraits::install_dir()),
      base::Bind(&MaybeRegisterComponent, cus, prefs));

  // Run the reporter if there is a pending execution request.
  int execute_try_count = prefs->GetInteger(prefs::kSwReporterExecuteTryCount);
  if (execute_try_count > 0) {
    // Retrieve the results if the pending request has completed.
    base::win::RegKey srt_key(
        HKEY_CURRENT_USER, kSoftwareRemovalToolRegistryKey, KEY_READ);
    DWORD exit_code;
    if (srt_key.Valid() &&
        srt_key.ReadValueDW(kExitCodeRegistryValueName, &exit_code) ==
            ERROR_SUCCESS) {
      ReportUmaStep(SW_REPORTER_REGISTRY_EXIT_CODE);
      ReportAndClearExitCode(exit_code);
      return;
    }

    // The previous request has not completed. The reporter will run again
    // when ComponentReady is called or the request is abandoned if it has
    // been tried too many times.
    prefs->SetInteger(prefs::kSwReporterExecuteTryCount, --execute_try_count);
    if (execute_try_count > 0)
      ReportUmaStep(SW_REPORTER_STARTUP_RETRY);
    else
      ReportUmaStep(SW_REPORTER_RETRIED_TOO_MANY_TIMES);
  }
}

void RegisterPrefsForSwReporter(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kSwReporterExecuteTryCount, 0);
}

}  // namespace component_updater
