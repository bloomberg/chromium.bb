// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_component_installer.h"

#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace component_updater {

namespace {

// CRX hash. The extension id is: npdjjkjlcidkjlamlmmdelcjbcpdjocm.
const uint8 kSha2Hash[] = {0xdf, 0x39, 0x9a, 0x9b, 0x28, 0x3a, 0x9b, 0x0c,
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

}  // namespace

class RecoveryComponentInstaller : public ComponentInstaller {
 public:
  explicit RecoveryComponentInstaller(const Version& version,
                                      PrefService* prefs);

  virtual ~RecoveryComponentInstaller() {}

  virtual void OnUpdateError(int error) OVERRIDE;

  virtual bool Install(const base::DictionaryValue& manifest,
                       const base::FilePath& unpack_path) OVERRIDE;

  virtual bool GetInstalledFile(const std::string& file,
                                base::FilePath* installed_file) OVERRIDE;

 private:
  Version current_version_;
  PrefService* prefs_;
};

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

RecoveryComponentInstaller::RecoveryComponentInstaller(const Version& version,
                                                       PrefService* prefs)
    : current_version_(version), prefs_(prefs) {
  DCHECK(version.IsValid());
}

void RecoveryComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Recovery component update error: " << error;
}

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
  CommandLine cmdline(main_file);
  std::string arguments;
  if (manifest.GetStringASCII("x-recovery-args", &arguments))
    cmdline.AppendArg(arguments);
  std::string add_version;
  if (manifest.GetStringASCII("x-recovery-add-version", &add_version)) {
    if (add_version == "yes")
      cmdline.AppendSwitchASCII("version", current_version_.GetString());
  }
  current_version_ = version;
  if (prefs_) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&RecoveryUpdateVersionHelper, version, prefs_));
  }
  return base::LaunchProcess(cmdline, base::LaunchOptions(), NULL);
}

bool RecoveryComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  return false;
}

void RegisterRecoveryComponent(ComponentUpdateService* cus,
                               PrefService* prefs) {
#if !defined(OS_CHROMEOS)
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
}

}  // namespace component_updater
