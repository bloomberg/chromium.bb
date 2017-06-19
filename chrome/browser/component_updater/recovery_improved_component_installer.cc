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
// The component id is: iddcipcljjhfegcfaaaapdilddpplalp
constexpr uint8_t kPublicKeySHA256[32] = {
    0x83, 0x32, 0x8f, 0x2b, 0x99, 0x75, 0x46, 0x25, 0x00, 0x00, 0xf3,
    0x8b, 0x33, 0xff, 0xb0, 0xbf, 0xea, 0xea, 0x19, 0xb3, 0x38, 0xfb,
    0xdc, 0xb3, 0x28, 0x90, 0x5f, 0xe2, 0xbe, 0x28, 0x89, 0x11};

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
