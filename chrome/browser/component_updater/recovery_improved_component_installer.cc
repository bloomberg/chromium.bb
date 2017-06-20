// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/recovery_improved_component_installer.h"

#include <iterator>
#include <utility>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/component_updater_utils.h"

// This component is behind a Finch experiment. To enable the registration of
// the component, run Chrome with --enable-features=ImprovedRecoveryComponent.
namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component CRX.
// The component id is: gjpmebpgbhcamgdgjcmnjfhggjpgcimm
constexpr uint8_t kPublicKeySHA256[32] = {
    0x69, 0xfc, 0x41, 0xf6, 0x17, 0x20, 0xc6, 0x36, 0x92, 0xcd, 0x95,
    0x76, 0x69, 0xf6, 0x28, 0xcc, 0xbe, 0x98, 0x4b, 0x93, 0x17, 0xd6,
    0x9c, 0xb3, 0x64, 0x0c, 0x0d, 0x25, 0x61, 0xc5, 0x80, 0x1d};

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
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "RecoveryImproved component is ready.";
}

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
  return "Chrome Improved Recovery";
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
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_WIN) || defined(OS_MACOSX)
  // The improved recovery components requires elevation in the case where
  // Chrome is installed per-machine. The elevation mechanism is not implemented
  // yet; therefore, the component is not registered in this case.
  if (!IsPerUserInstall())
    return;

  DVLOG(1) << "Registering RecoveryImproved component.";

  std::unique_ptr<ComponentInstallerTraits> traits(
      new RecoveryImprovedInstallerTraits(prefs));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
#endif
#endif
}

void RegisterPrefsForRecoveryImprovedComponent(PrefRegistrySimple* registry) {}

}  // namespace component_updater
