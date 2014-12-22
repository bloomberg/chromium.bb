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
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/threading/worker_pool.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
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

#if !defined(OS_CHROMEOS)
// Checks if elevated recovery simulation switch was present on the command
// line. This is for testing purpose.
bool SimulatingElevatedRecovery() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSimulateElevatedRecovery);
}
#endif

#if defined(OS_WIN)
scoped_ptr<base::DictionaryValue> ReadManifest(const base::FilePath& manifest) {
  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  if (root.get() && root->IsType(base::Value::TYPE_DICTIONARY)) {
    return scoped_ptr<base::DictionaryValue>(
        static_cast<base::DictionaryValue*>(root.release()));
  }
  return scoped_ptr<base::DictionaryValue>();
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

  base::CommandLine cmdline(main_file);
  std::string arguments;
  if (manifest->GetStringASCII("x-recovery-args", &arguments))
    cmdline.AppendArg(arguments);
  std::string add_version;
  if (manifest->GetStringASCII("x-recovery-add-version", &add_version) &&
      add_version == "yes") {
    cmdline.AppendSwitchASCII("version", version.GetString());
  }

  base::LaunchOptions options;
  options.start_hidden = true;
  base::LaunchElevatedProcess(cmdline, options);
}

void ElevatedInstallRecoveryComponent(const base::FilePath& installer_path) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&DoElevatedInstallRecoveryComponent, installer_path),
      true);
}
#endif

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
class RecoveryComponentInstaller : public ComponentInstaller {
 public:
  RecoveryComponentInstaller(const Version& version, PrefService* prefs);
  ~RecoveryComponentInstaller() override {}

  void OnUpdateError(int error) override;

  bool Install(const base::DictionaryValue& manifest,
               const base::FilePath& unpack_path) override;

  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

 private:
  bool RunInstallCommand(const base::CommandLine& cmdline,
                         const base::FilePath& installer_folder) const;

  Version current_version_;
  PrefService* prefs_;
};

void SimulateElevatedRecoveryHelper(PrefService* prefs) {
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

void RecoveryRegisterHelper(ComponentUpdateService* cus, PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Version version(prefs->GetString(prefs::kRecoveryComponentVersion));
  if (!version.IsValid()) {
    NOTREACHED();
    return;
  }

  CrxComponent recovery;
  recovery.name = "recovery";
  recovery.installer = new RecoveryComponentInstaller(version, prefs);
  recovery.version = version;
  recovery.pk_hash.assign(kSha2Hash, &kSha2Hash[sizeof(kSha2Hash)]);
  if (cus->RegisterComponent(recovery) != ComponentUpdateService::kOk) {
    NOTREACHED() << "Recovery component registration failed.";
  }
}

void RecoveryUpdateVersionHelper(const Version& version, PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs->SetString(prefs::kRecoveryComponentVersion, version.GetString());
}

void SetPrefsForElevatedRecoveryInstall(const base::FilePath& unpack_path,
                                        PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs->SetFilePath(prefs::kRecoveryComponentUnpackPath, unpack_path);
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, true);
}

RecoveryComponentInstaller::RecoveryComponentInstaller(const Version& version,
                                                       PrefService* prefs)
    : current_version_(version), prefs_(prefs) {
  DCHECK(version.IsValid());
}

void RecoveryComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Recovery component update error: " << error;
}

#if defined(OS_WIN)
void WaitForInstallToComplete(base::ProcessHandle process_handle,
                              const base::FilePath& installer_folder,
                              PrefService* prefs) {
  int installer_exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  if (base::WaitForExitCodeWithTimeout(process_handle,
                                       &installer_exit_code,
                                       kMaxWaitTime) &&
      installer_exit_code == EXIT_CODE_ELEVATION_NEEDED) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SetPrefsForElevatedRecoveryInstall,
                   installer_folder,
                   prefs));
  }
  base::CloseProcessHandle(process_handle);
}

bool RecoveryComponentInstaller::RunInstallCommand(
    const base::CommandLine& cmdline,
    const base::FilePath& installer_folder) const {
  base::ProcessHandle process_handle;
  base::LaunchOptions options;
  options.start_hidden = true;
  if (!base::LaunchProcess(cmdline, options, &process_handle))
    return false;

  // Let worker pool thread wait for us so we don't block Chrome shutdown.
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&WaitForInstallToComplete,
                 process_handle, installer_folder, prefs_),
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
  return base::LaunchProcess(cmdline, base::LaunchOptions(), NULL);
}
#endif

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
  // Run the recovery component.
  base::CommandLine cmdline(main_file);
  std::string arguments;
  if (manifest.GetStringASCII("x-recovery-args", &arguments))
    cmdline.AppendArg(arguments);
  std::string add_version;
  if (manifest.GetStringASCII("x-recovery-add-version", &add_version) &&
      add_version == "yes") {
    cmdline.AppendSwitchASCII("version", current_version_.GetString());
  }

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
#endif
}

void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kRecoveryComponentVersion, "0.0.0.0");
  registry->RegisterFilePathPref(prefs::kRecoveryComponentUnpackPath,
                                 base::FilePath());
  registry->RegisterBooleanPref(prefs::kRecoveryComponentNeedsElevation, false);
}

void AcceptedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(OS_WIN)
  ElevatedInstallRecoveryComponent(
      prefs->GetFilePath(prefs::kRecoveryComponentUnpackPath));
#endif
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

void DeclinedElevatedRecoveryInstall(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs->SetBoolean(prefs::kRecoveryComponentNeedsElevation, false);
}

}  // namespace component_updater
