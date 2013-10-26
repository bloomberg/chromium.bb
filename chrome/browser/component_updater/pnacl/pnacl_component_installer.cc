// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "content/public/browser/browser_thread.h"

using chrome::OmahaQueryParams;
using content::BrowserThread;

namespace {

// Name of the Pnacl component specified in the manifest.
const char kPnaclManifestName[] = "PNaCl Translator";

// Sanitize characters from Pnacl Arch value so that they can be used
// in path names.  This should only be characters in the set: [a-z0-9_].
// Keep in sync with chrome/browser/nacl_host/nacl_file_host.
std::string SanitizeForPath(const std::string& input) {
  std::string result;
  ReplaceChars(input, "-", "_", &result);
  return result;
}

// Set the component's hash to the multi-CRX PNaCl package.
void SetPnaclHash(CrxComponent* component) {
 static const uint8 sha256_hash[32] =
     { // This corresponds to AppID: hnimpnehoodheedghdeeijklkeaacbdc
       0x7d, 0x8c, 0xfd, 0x47, 0xee, 0x37, 0x44, 0x36, 0x73, 0x44,
       0x89, 0xab, 0xa4, 0x00, 0x21, 0x32, 0x4a, 0x06, 0x06, 0xf1, 0x51,
       0x3c, 0x51, 0xba, 0x31, 0x2f, 0xbc, 0xb3, 0x99, 0x07, 0xdc, 0x9c};

  component->pk_hash.assign(sha256_hash,
                            &sha256_hash[arraysize(sha256_hash)]);
}

// If we don't have Pnacl installed, this is the version we claim.
const char kNullVersion[] = "0.0.0.0";

// PNaCl is packaged as a multi-CRX.  This returns the platform-specific
// subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDir(const base::FilePath& base_path) {
  std::string arch = SanitizeForPath(OmahaQueryParams::getNaclArch());
  return base_path.AppendASCII("_platform_specific").AppendASCII(arch);
}

// Tell the rest of the world where to find the platform-specific PNaCl files.
void OverrideDirPnaclComponent(const base::FilePath& base_path) {
  PathService::Override(chrome::DIR_PNACL_COMPONENT,
                        GetPlatformDir(base_path));
}

bool GetLatestPnaclDirectory(PnaclComponentInstaller* pci,
                             base::FilePath* latest_dir,
                             Version* latest_version,
                             std::vector<base::FilePath>* older_dirs) {
  // Enumerate all versions starting from the base directory.
  base::FilePath base_dir = pci->GetPnaclBaseDirectory();
  bool found = false;
  base::FileEnumerator
      file_enumerator(base_dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    Version version(path.BaseName().MaybeAsASCII());
    if (!version.IsValid())
      continue;
    if (found) {
      if (version.CompareTo(*latest_version) > 0) {
        older_dirs->push_back(*latest_dir);
        *latest_dir = path;
        *latest_version = version;
      } else {
        older_dirs->push_back(path);
      }
    } else {
      *latest_version = version;
      *latest_dir = path;
      found = true;
    }
  }
  return found;
}

// Read a manifest file in.
base::DictionaryValue* ReadJSONManifest(
    const base::FilePath& manifest_path) {
  JSONFileValueSerializer serializer(manifest_path);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  if (!root.get())
    return NULL;
  if (!root->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;
  return static_cast<base::DictionaryValue*>(root.release());
}

// Read the PNaCl specific manifest.
base::DictionaryValue* ReadPnaclManifest(const base::FilePath& unpack_path) {
  base::FilePath manifest_path = GetPlatformDir(unpack_path).AppendASCII(
      "pnacl_public_pnacl_json");
  if (!base::PathExists(manifest_path))
    return NULL;
  return ReadJSONManifest(manifest_path);
}

// Read the component's manifest.json.
base::DictionaryValue* ReadComponentManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest_path = unpack_path.Append(
      FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest_path))
    return NULL;
  return ReadJSONManifest(manifest_path);
}

// Check that the component's manifest is for PNaCl, and check the
// PNaCl manifest indicates this is the correct arch-specific package.
bool CheckPnaclComponentManifest(const base::DictionaryValue& manifest,
                                 const base::DictionaryValue& pnacl_manifest,
                                 Version* version_out) {
  // Make sure we have the right |manifest| file.
  std::string name;
  if (!manifest.GetStringASCII("name", &name)) {
    LOG(WARNING) << "'name' field is missing from manifest!";
    return false;
  }
  // For the webstore, we've given different names to each of the
  // architecture specific packages (and test/QA vs not test/QA)
  // so only part of it is the same.
  if (name.find(kPnaclManifestName) == std::string::npos) {
    LOG(WARNING) << "'name' field in manifest is invalid ("
                 << name << ") -- missing ("
                 << kPnaclManifestName << ")";
    return false;
  }

  std::string proposed_version;
  if (!manifest.GetStringASCII("version", &proposed_version)) {
    LOG(WARNING) << "'version' field is missing from manifest!";
    return false;
  }
  Version version(proposed_version.c_str());
  if (!version.IsValid()) {
    LOG(WARNING) << "'version' field in manifest is invalid "
                 << version.GetString();
    return false;
  }

  // Now check the |pnacl_manifest|.
  std::string arch;
  if (!pnacl_manifest.GetStringASCII("pnacl-arch", &arch)) {
    LOG(WARNING) << "'pnacl-arch' field is missing from pnacl-manifest!";
    return false;
  }
  if (arch.compare(OmahaQueryParams::getNaclArch()) != 0) {
    LOG(WARNING) << "'pnacl-arch' field in manifest is invalid ("
                 << arch << " vs " << OmahaQueryParams::getNaclArch() << ")";
    return false;
  }

  *version_out = version;
  return true;
}

}  // namespace

PnaclComponentInstaller::PnaclComponentInstaller()
    : per_user_(false),
      updates_disabled_(false),
      cus_(NULL) {
#if defined(OS_CHROMEOS)
  per_user_ = true;
#endif
  updater_observer_.reset(new PnaclUpdaterObserver(this));
}

PnaclComponentInstaller::~PnaclComponentInstaller() {
}

void PnaclComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Pnacl update error: " << error;
}

// Pnacl components have the version encoded in the path itself:
// <profile>\AppData\Local\Google\Chrome\User Data\pnacl\0.1.2.3\.
// and the base directory will be:
// <profile>\AppData\Local\Google\Chrome\User Data\pnacl\.
base::FilePath PnaclComponentInstaller::GetPnaclBaseDirectory() {
  // For ChromeOS, temporarily make this user-dependent (for integrity) until
  // we find a better solution.
  // This is not ideal because of the following:
  //   (a) We end up with per-user copies instead of a single copy
  //   (b) The profile can change as users log in to different accounts
  //   so we need to watch for user-login-events (see pnacl_profile_observer.h).
  if (per_user_) {
    DCHECK(!current_profile_path_.empty());
    base::FilePath path = current_profile_path_.Append(
        FILE_PATH_LITERAL("pnacl"));
    return path;
  } else {
    base::FilePath result;
    CHECK(PathService::Get(chrome::DIR_PNACL_BASE, &result));
    return result;
  }
}

void PnaclComponentInstaller::OnProfileChange() {
  // On chromeos, we want to find the --login-profile=<foo> dir.
  // Even though the path does vary between users, the content
  // changes when logging out and logging in.
  ProfileManager* pm = g_browser_process->profile_manager();
  current_profile_path_ = pm->user_data_dir().Append(
      pm->GetInitialProfileDir());
}

bool PnaclComponentInstaller::Install(const base::DictionaryValue& manifest,
                                      const base::FilePath& unpack_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<base::DictionaryValue> pnacl_manifest(
      ReadPnaclManifest(unpack_path));
  if (pnacl_manifest == NULL) {
    LOG(WARNING) << "Failed to read pnacl manifest.";
    NotifyInstallError();
    return false;
  }

  Version version;
  if (!CheckPnaclComponentManifest(manifest, *pnacl_manifest, &version)) {
    LOG(WARNING) << "CheckPnaclComponentManifest failed, not installing.";
    NotifyInstallError();
    return false;
  }

  // Don't install if the current version is actually newer.
  if (current_version().CompareTo(version) > 0) {
    NotifyInstallError();
    return false;
  }

  // Passed the basic tests. Time to install it.
  base::FilePath path = GetPnaclBaseDirectory().AppendASCII(
      version.GetString());
  if (base::PathExists(path)) {
    LOG(WARNING) << "Target path already exists, not installing.";
    NotifyInstallError();
    return false;
  }
  if (!base::Move(unpack_path, path)) {
    LOG(WARNING) << "Move failed, not installing.";
    NotifyInstallError();
    return false;
  }

  // Installation is done. Now tell the rest of chrome.
  // - The path service.
  // - Callbacks that requested an update.
  set_current_version(version);
  NotifyInstallSuccess();
  OverrideDirPnaclComponent(path);
  return true;
}

// Given |file|, which can be a path like "_platform_specific/arm/pnacl_foo",
// returns the assumed install path. The path separator in |file| is '/'
// for all platforms. Caller is responsible for checking that the
// |installed_file| actually exists.
bool PnaclComponentInstaller::GetInstalledFile(
    const std::string& file, base::FilePath* installed_file) {
  if (current_version().Equals(Version(kNullVersion)))
    return false;

  *installed_file = GetPnaclBaseDirectory().AppendASCII(
      current_version().GetString()).AppendASCII(file);
  return true;
}

void PnaclComponentInstaller::NotifyInstallError() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PnaclComponentInstaller::NotifyInstallError,
                   // Unretained because installer lives until process shutdown.
                   base::Unretained(this)));
    return;
  }
  if (!install_callback_.is_null()) {
    updater_observer_->StopObserving();
    install_callback_.Run(false);
    install_callback_.Reset();
  }
}

void PnaclComponentInstaller::NotifyInstallSuccess() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&PnaclComponentInstaller::NotifyInstallSuccess,
                   // Unretained because installer lives until process shutdown.
                   base::Unretained(this)));
    return;
  }
  if (!install_callback_.is_null()) {
    updater_observer_->StopObserving();
    install_callback_.Run(true);
    install_callback_.Reset();
  }
}

CrxComponent PnaclComponentInstaller::GetCrxComponent() {
  CrxComponent pnacl_component;
  pnacl_component.version = current_version();
  pnacl_component.name = "pnacl";
  pnacl_component.installer = this;
  pnacl_component.observer = updater_observer_.get();
  pnacl_component.fingerprint = current_fingerprint();
  SetPnaclHash(&pnacl_component);

  return pnacl_component;
}

namespace {

void FinishPnaclUpdateRegistration(const Version& current_version,
                                   const std::string& current_fingerprint,
                                   PnaclComponentInstaller* pci) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pci->set_current_version(current_version);
  pci->set_current_fingerprint(current_fingerprint);
  CrxComponent pnacl_component = pci->GetCrxComponent();

  ComponentUpdateService::Status status =
      pci->cus()->RegisterComponent(pnacl_component);
  if (status != ComponentUpdateService::kOk
      && status != ComponentUpdateService::kReplaced) {
    NOTREACHED() << "Pnacl component registration failed.";
  }
}

// Check if there is an existing version on disk first to know when
// a hosted version is actually newer.
void StartPnaclUpdateRegistration(PnaclComponentInstaller* pci) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = pci->GetPnaclBaseDirectory();
  if (!base::PathExists(path)) {
    if (!file_util::CreateDirectory(path)) {
      NOTREACHED() << "Could not create base Pnacl directory.";
      return;
    }
  }

  Version current_version(kNullVersion);
  std::string current_fingerprint;
  std::vector<base::FilePath> older_dirs;
  if (GetLatestPnaclDirectory(pci, &path, &current_version, &older_dirs)) {
    scoped_ptr<base::DictionaryValue> manifest(
        ReadComponentManifest(path));
    scoped_ptr<base::DictionaryValue> pnacl_manifest(
        ReadPnaclManifest(path));
    Version manifest_version;
    // Check that the component manifest and PNaCl manifest files
    // are legit, and that the indicated version matches the one
    // encoded within the path name.
    if (manifest == NULL || pnacl_manifest == NULL
        || !CheckPnaclComponentManifest(*manifest,
                                        *pnacl_manifest,
                                        &manifest_version)
        || !current_version.Equals(manifest_version)) {
      current_version = Version(kNullVersion);
    } else {
      OverrideDirPnaclComponent(path);
      base::ReadFileToString(path.AppendASCII("manifest.fingerprint"),
                             &current_fingerprint);
    }
  }

  // If updates are disabled, only discover the current version
  // and OverrideDirPnaclComponent. That way, developers can use
  // a pinned version. Do not actually finish registration with
  // the component update service.
  if (pci->updates_disabled())
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FinishPnaclUpdateRegistration,
                 current_version,
                 current_fingerprint,
                 pci));

  // Remove older versions of PNaCl.
  for (std::vector<base::FilePath>::iterator iter = older_dirs.begin();
       iter != older_dirs.end(); ++iter) {
    base::DeleteFile(*iter, true);
  }
}

// Remove old per-profile copies of PNaCl (was for ChromeOS).
// TODO(jvoung): Delete this code once most ChromeOS users have reaped
// their old per-profile copies of PNaCl.
void ReapOldChromeOSPnaclFiles(PnaclComponentInstaller* pci) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath path = pci->GetPnaclBaseDirectory();
  if (!base::PathExists(path))
    return;

  // Do a basic sanity check first.
  if (pci->per_user()
      && path.BaseName().value().compare(FILE_PATH_LITERAL("pnacl")) == 0)
    base::DeleteFile(path, true);
}


void GetProfileInformation(PnaclComponentInstaller* pci) {
  // Bail if not logged in yet.
  if (!g_browser_process->profile_manager()->IsLoggedIn()) {
    return;
  }

  pci->OnProfileChange();

  // Do not actually register PNaCl for component updates, for CHROMEOS.
  // Just get the profile information and delete the per-profile files
  // if they exist.
 BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                         base::Bind(&ReapOldChromeOSPnaclFiles, pci));
}

}  // namespace

void PnaclComponentInstaller::RegisterPnaclComponent(
                            ComponentUpdateService* cus,
                            const CommandLine& command_line) {
  // Register PNaCl by default (can be disabled).
  updates_disabled_ = command_line.HasSwitch(switches::kDisablePnaclInstall);
  cus_ = cus;
  // If per_user, create a profile observer to watch for logins.
  // Only do so after cus_ is set to something non-null.
  if (per_user_ && !profile_observer_) {
    profile_observer_.reset(new PnaclProfileObserver(this));
  }
  if (per_user_) {
    // Figure out profile information, before proceeding to look for files.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&GetProfileInformation, this));
  } else {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&StartPnaclUpdateRegistration, this));
  }
}

void PnaclComponentInstaller::ReRegisterPnacl() {
  DCHECK(per_user_);
  // Figure out profile information, before proceeding to look for files.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetProfileInformation, this));
}

void PnaclComponentInstaller::RequestFirstInstall(const InstallCallback& cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Only one request can happen at once.
  if (!install_callback_.is_null()) {
    cb.Run(false);
    return;
  }
  set_current_version(Version(kNullVersion));
  CrxComponent pnacl_component = GetCrxComponent();
  ComponentUpdateService::Status status = cus_->OnDemandUpdate(
      GetCrxComponentID(pnacl_component));
  if (status != ComponentUpdateService::kOk) {
    cb.Run(false);
    return;
  }
  install_callback_ = cb;
  updater_observer_->EnsureObserving();
}
