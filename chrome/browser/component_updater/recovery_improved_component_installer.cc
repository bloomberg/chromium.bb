// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include <iterator>
#include <utility>

#include "base/callback.h"

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component CRX.
// The extension id is: iddcipcljjhfegcfaaaapdilddpplalp
constexpr uint8_t kPublicKeySHA256[32] = {
    0x97, 0xf0, 0xbe, 0xe4, 0x3f, 0x2b, 0x9e, 0xcf, 0x2c, 0x50, 0x61,
    0xdf, 0xc2, 0x6e, 0x0b, 0x4a, 0x4f, 0x1e, 0xda, 0x71, 0x29, 0x64,
    0x74, 0x70, 0x15, 0x07, 0x18, 0xb7, 0x92, 0x04, 0xcd, 0x70};

constexpr char kRecoveryImprovedManifestName[] = "Chrome Improved Recovery";

RecoveryImprovedInstallerTraits::RecoveryImprovedInstallerTraits(
    PrefService* prefs)
    : prefs_(prefs) {}

RecoveryImprovedInstallerTraits::~RecoveryImprovedInstallerTraits() {}

bool RecoveryImprovedInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool RecoveryImprovedInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
RecoveryImprovedInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void RecoveryImprovedInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {}

// Called during startup and installation before ComponentReady().
bool RecoveryImprovedInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath RecoveryImprovedInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("RecoveryImproved"));
}

void RecoveryImprovedInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string RecoveryImprovedInstallerTraits::GetName() const {
  return kRecoveryImprovedManifestName;
}

update_client::InstallerAttributes
RecoveryImprovedInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> RecoveryImprovedInstallerTraits::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterRecoveryImprovedComponent(ComponentUpdateService* cus,
                                       PrefService* prefs) {
  DVLOG(1) << "Registering RecoveryImproved component.";

  std::unique_ptr<ComponentInstallerTraits> traits(
      new RecoveryImprovedInstallerTraits(prefs));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

void RegisterPrefsForRecoveryImprovedComponent(PrefRegistrySimple* registry) {}

}  // namespace component_updater
