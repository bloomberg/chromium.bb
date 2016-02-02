// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_component_installer.h"

#include <stdint.h>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// CRX hash. The extension id is: npdjjkjlcidkjlamlmmdelcjbcpdjocm.
const uint8_t kSha2Hash[] = {0xdf, 0x39, 0x9a, 0x9b, 0x28, 0x3a, 0x9b, 0x0c,
                             0xbc, 0xc3, 0x4b, 0x29, 0x12, 0xf3, 0x9e, 0x2c,
                             0x19, 0x7a, 0x71, 0x4b, 0x0a, 0x7c, 0x80, 0x1c,
                             0xf6, 0x29, 0x7c, 0x0a, 0x5f, 0xea, 0x67, 0xb7};

// File name of the recovery binary on different platforms.
const base::FilePath::CharType kRecoveryFileName[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("ChromeRecovery.exe");
#else  // OS_LINUX, OS_MACOSX, etc.
    FILE_PATH_LITERAL("ChromeRecovery");
#endif

const char kRecoveryManifestName[] = "ChromeRecovery";

// ChromeRecovery process exit codes.
enum ChromeRecoveryExitCode {
  EXIT_CODE_RECOVERY_SUCCEEDED = 0,
  EXIT_CODE_RECOVERY_SKIPPED = 1,
  EXIT_CODE_ELEVATION_NEEDED = 2,
};

enum RecoveryComponentEvent {
  RCE_RUNNING_NON_ELEVATED = 0,
  RCE_ELEVATION_NEEDED = 1,
  RCE_FAILED = 2,
  RCE_SUCCEEDED = 3,
  RCE_SKIPPED = 4,
  RCE_RUNNING_ELEVATED = 5,
  RCE_ELEVATED_FAILED = 6,
  RCE_ELEVATED_SUCCEEDED = 7,
  RCE_ELEVATED_SKIPPED = 8,
  RCE_COMPONENT_DOWNLOAD_ERROR = 9,
  RCE_COUNT
};

void RecordRecoveryComponentUMAEvent(RecoveryComponentEvent event) {
  UMA_HISTOGRAM_ENUMERATION("RecoveryComponent.Event", event, RCE_COUNT);
}

#if !defined(OS_CHROMEOS)
// Checks if elevated recovery simulation switch was present on the command
// line. This is for testing purpose.
bool SimulatingElevatedRecovery() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSimulateElevatedRecovery);
}
#endif  // !defined(OS_CHROMEOS)

base::CommandLine GetRecoveryInstallCommandLine(
    const base::FilePath& command,
    const base::DictionaryValue& manifest,
    bool is_deferred_run,
    const Version& version) {
  base::CommandLine command_line(command);

  // Add a flag to for re-attempted install with elevated privilege so that the
  // recovery executable can report back accordingly.
  if (is_deferred_run)
    command_line.AppendArg("/deferredrun");

  std::string arguments;
  if (manifest.GetStringASCII("x-recovery-args", &arguments))
    command_line.AppendArg(arguments);
  std::string add_version;
  if (manifest.GetStringASCII("x-recovery-add-version", &add_version) &&
      add_version == "yes") {
    std::string version_string = "/version ";
    version_string += version.GetString();
    command_line.AppendArg(version_string);
  }

  return command_line;
}

#if defined(OS_WIN)
scoped_ptr<base::DictionaryValue> ReadManifest(const base::FilePath& manifest) {
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  return base::DictionaryValue::From(deserializer.Deserialize(NULL, &error));
}

void WaitForElevatedInstallToComplete(base::Process process) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  if (process.WaitForExitWithTimeout(kMaxWaitTime, &installer_exit_code)) {
    if (installer_exit_code == EXIT_CODE_RECOVERY_SUCCEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATED_SUCCEEDED);
    } else {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATED_SKIPPED);
    }
  } else {
    RecordRecoveryComponentUMAEvent(RCE_ELEVATED_FAILED);
  }
}

void DoElevatedInstallRecoveryComponent(const base::FilePath& path) {
  const base::FilePath main_file = path.Append(kRecoveryFileName);
  const base::FilePath manifest_file =
      path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(main_file) || !base::PathExists(manifest_file))
    return;

  scoped_ptr<base::DictionaryValue> manifest(ReadManifest(manifest_file));
  std::string name;
  manifest->GetStringASCII("name", &name);
  if (name != kRecoveryManifestName)
    return;
  std::string proposed_version;
  manifest->GetStringASCII("version", &proposed_version);
  const Version version(proposed_version.c_str());
  if (!version.IsValid())
    return;

  const bool is_deferred_run = true;
  const auto cmdline = GetRecoveryInstallCommandLine(
      main_file, *manifest, is_deferred_run, version);

  RecordRecoveryComponentUMAEvent(RCE_RUNNING_ELEVATED);

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchElevatedProcess(cmdline, options);

  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WaitForElevatedInstallToComplete, base::Passed(&process)),
      true);
}

void ElevatedInstallRecoveryComponent(const base::FilePath& installer_path) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&DoElevatedInstallRecoveryComponent, installer_path),
      true);
}
#endif  // defined(OS_WIN)

}  // namespace

// Component installer that is responsible to repair the chrome installation
// or repair the Google update installation. This is a last resort safety
// mechanism.
// For user Chrome, recovery component just installs silently. For machine
// Chrome, elevation may be needed. If that happens, the installer will set
// preference flag prefs::kRecoveryComponentNeedsElevation to request that.
// There is a global error service monitors this flag and will pop up
// bubble if the flag is set to true.
// See chrome/browser/recovery/recovery_install_global_error.cc for details.
class RecoveryComponentInstaller : public update_client::CrxInstaller {
 public:
  RecoveryComponentInstaller(const Version& version, PrefService* prefs);

  // ComponentInstaller implementation:
  void OnUpdateError(int error) override;

  bool Install(const base::DictionaryValue& manifest,
               const base::FilePath& unpack_path) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

  bool Uninstall() override;

 private:
  ~RecoveryComponentInstaller() override {}

  bool RunInstallCommand(const base::CommandLine& cmdline,
                         const base::FilePath& installer_folder) const;

  Version current_version_;
  PrefService* prefs_;
};

void SimulateElevatedRecoveryHelper(PrefService* prefs) {
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

void RecoveryRegisterHelper(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Version version(prefs->GetString(prefs::kRecoveryComponentVersion));
  if (!version.IsValid()) {
    NOTREACHED();
    return;
  }

  update_client::CrxComponent recovery;
  recovery.name = "recovery";
  recovery.installer = new RecoveryComponentInstaller(version, prefs);
  recovery.version = version;
  recovery.pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);
  if (!cus->RegisterComponent(recovery)) {
    NOTREACHED() << "Recovery component registration failed.";
  }
}

void RecoveryUpdateVersionHelper(const Version& version, PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetString(prefs::kRecoveryComponentVersion, version.GetString());
}

void SetPrefsForElevatedRecoveryInstall(const base::FilePath& unpack_path,
                                        PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetFilePath(prefs::kRecoveryComponentUnpackPath, unpack_path);
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

RecoveryComponentInstaller::RecoveryComponentInstaller(const Version& version,
                                                       PrefService* prefs)
    : current_version_(version), prefs_(prefs) {
  DCHECK(version.IsValid());
}

void RecoveryComponentInstaller::OnUpdateError(int error) {
  RecordRecoveryComponentUMAEvent(RCE_COMPONENT_DOWNLOAD_ERROR);
  NOTREACHED() << "Recovery component update error: " << error;
}

#if defined(OS_WIN)
void WaitForInstallToComplete(base::Process process,
                              const base::FilePath& installer_folder,
                              PrefService* prefs) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  if (process.WaitForExitWithTimeout(kMaxWaitTime, &installer_exit_code)) {
    if (installer_exit_code == EXIT_CODE_ELEVATION_NEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_ELEVATION_NEEDED);

      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SetPrefsForElevatedRecoveryInstall,
                     installer_folder,
                     prefs));
    } else if (installer_exit_code == EXIT_CODE_RECOVERY_SUCCEEDED) {
      RecordRecoveryComponentUMAEvent(RCE_SUCCEEDED);
    } else if (installer_exit_code == EXIT_CODE_RECOVERY_SKIPPED) {
      RecordRecoveryComponentUMAEvent(RCE_SKIPPED);
    }
  } else {
    RecordRecoveryComponentUMAEvent(RCE_FAILED);
  }
}

bool RecoveryComponentInstaller::RunInstallCommand(
    const base::CommandLine& cmdline,
    const base::FilePath& installer_folder) const {
  RecordRecoveryComponentUMAEvent(RCE_RUNNING_NON_ELEVATED);

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid())
    return false;

  // Let worker pool thread wait for us so we don't block Chrome shutdown.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WaitForInstallToComplete,
                 base::Passed(&process), installer_folder, prefs_),
      true);

  // Returns true regardless of install result since from updater service
  // perspective the install is done, even we may need to do elevated
  // install later.
  return true;
}
#else
bool RecoveryComponentInstaller::RunInstallCommand(
    const base::CommandLine& cmdline,
    const base::FilePath&) const {
  return base::LaunchProcess(cmdline, base::LaunchOptions()).IsValid();
}
#endif  // defined(OS_WIN)

#if defined(OS_POSIX)
// Sets the POSIX executable permissions on a file
bool SetPosixExecutablePermission(const base::FilePath& path) {
  int permissions = 0;
  if (!base::GetPosixFilePermissions(path, &permissions))
    return false;
  const int kExecutableMask = base::FILE_PERMISSION_EXECUTE_BY_USER |
                              base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                              base::FILE_PERMISSION_EXECUTE_BY_OTHERS;
  if ((permissions & kExecutableMask) == kExecutableMask)
    return true;  // No need to update
  return base::SetPosixFilePermissions(path, permissions | kExecutableMask);
}
#endif  // defined(OS_POSIX)

bool RecoveryComponentInstaller::Install(const base::DictionaryValue& manifest,
                                         const base::FilePath& unpack_path) {
  std::string name;
  manifest.GetStringASCII("name", &name);
  if (name != kRecoveryManifestName)
    return false;
  std::string proposed_version;
  manifest.GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) >= 0)
    return false;

  // Passed the basic tests. Copy the installation to a permanent directory.
  base::FilePath path;
  if (!PathService::Get(DIR_RECOVERY_BASE, &path))
    return false;
  if (!base::PathExists(path) && !base::CreateDirectory(path))
    return false;
  path = path.AppendASCII(version.GetString());
  if (base::PathExists(path) && !base::DeleteFile(path, true))
    return false;
  if (!base::Move(unpack_path, path)) {
    DVLOG(1) << "Recovery component move failed.";
    return false;
  }

  base::FilePath main_file = path.Append(kRecoveryFileName);
  if (!base::PathExists(main_file))
    return false;

#if defined(OS_POSIX)
  // The current version of the CRX unzipping does not restore
  // correctly the executable flags/permissions. See https://crbug.com/555011
  if (!SetPosixExecutablePermission(main_file)) {
    DVLOG(1) << "Recovery component failed to set the executable "
                "permission on the file: "
             << main_file.value();
    return false;
  }
#endif

  // Run the recovery component.
  const bool is_deferred_run = false;
  const auto cmdline = GetRecoveryInstallCommandLine(
      main_file, manifest, is_deferred_run, current_version_);

  if (!RunInstallCommand(cmdline, path)) {
    return false;
  }

  current_version_ = version;
  if (prefs_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&RecoveryUpdateVersionHelper, version, prefs_));
  }
  return true;
}

bool RecoveryComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  return false;
}

bool RecoveryComponentInstaller::Uninstall() {
  return false;
}

void RegisterRecoveryComponent(ComponentUpdateService* cus,
                               PrefService* prefs) {
#if !defined(OS_CHROMEOS)
  if (SimulatingElevatedRecovery()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SimulateElevatedRecoveryHelper, prefs));
  }

  // We delay execute the registration because we are not required in
  // the critical path during browser startup.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RecoveryRegisterHelper, cus, prefs),
      base::TimeDelta::FromSeconds(6));
#endif  // !defined(OS_CHROMEOS)
}

void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRecoveryComponentVersion, "0.0.0.0");
  registry->RegisterFilePathPref(prefs::kRecoveryComponentUnpackPath,
                                 base::FilePath());
  registry->RegisterBooleanPref(prefs::kRecoveryComponentNeedsElevation, false);
}

void AcceptedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_WIN)
  ElevatedInstallRecoveryComponent(
      prefs->GetFilePath(prefs::kRecoveryComponentUnpackPath));
#endif  // OS_WIN
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

void DeclinedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

}  // namespace component_updater
