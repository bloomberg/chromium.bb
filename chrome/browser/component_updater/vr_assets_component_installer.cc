// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/vr_assets_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#include "base/version.h"
#include "chrome/browser/vr/assets.h"
#include "chrome/browser/vr/metrics_helper.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/component_updater/component_updater_paths.h"

using component_updater::ComponentUpdateService;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: cjfkbpdpjpdldhclahpfgnlhpodlpnba
const uint8_t kPublicKeySHA256[32] = {
    0x29, 0x5a, 0x1f, 0x3f, 0x9f, 0x3b, 0x37, 0x2b, 0x07, 0xf5, 0x6d,
    0xb7, 0xfe, 0x3b, 0xfd, 0x10, 0xb6, 0x80, 0xf3, 0x66, 0x0d, 0xc3,
    0xe2, 0x07, 0x25, 0x8d, 0x37, 0x85, 0x39, 0x51, 0x58, 0xcf};

const char kVrAssetsComponentName[] = "VR Assets";
const base::FilePath::CharType kRelativeInstallDir[] =
    FILE_PATH_LITERAL("VrAssets");

}  // namespace

namespace component_updater {

bool VrAssetsComponentInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool VrAssetsComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
VrAssetsComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void VrAssetsComponentInstallerTraits::OnCustomUninstall() {}

// Called during startup and installation before ComponentReady().
bool VrAssetsComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  auto* version_value = manifest.FindKey("version");
  if (!version_value || !version_value->is_string()) {
    vr::Assets::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kInvalid, base::nullopt);
    return false;
  }

  auto version_string = version_value->GetString();
  base::Version version(version_string);
  if (!version.IsValid() || version.components().size() != 2 ||
      !base::PathExists(install_dir)) {
    vr::Assets::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kInvalid, base::nullopt);
    return false;
  }

  if (version.components()[0] != vr::kCompatibleMajorVrAssetsComponentVersion) {
    vr::Assets::GetInstance()->GetMetricsHelper()->OnComponentUpdated(
        vr::AssetsComponentUpdateStatus::kIncompatible, version);
    return false;
  }

  return true;
}

void VrAssetsComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  vr::Assets::GetInstance()->OnComponentReady(version, install_dir,
                                              std::move(manifest));
}

base::FilePath VrAssetsComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(kRelativeInstallDir);
}

void VrAssetsComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + arraysize(kPublicKeySHA256));
}

std::string VrAssetsComponentInstallerTraits::GetName() const {
  return kVrAssetsComponentName;
}

update_client::InstallerAttributes
VrAssetsComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> VrAssetsComponentInstallerTraits::GetMimeTypes()
    const {
  return std::vector<std::string>();
}

void RegisterVrAssetsComponent(ComponentUpdateService* cus) {
  std::unique_ptr<ComponentInstallerPolicy> policy(
      new VrAssetsComponentInstallerTraits());
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));
  installer->Register(cus, base::Closure());
  vr::Assets::GetInstance()->GetMetricsHelper()->OnRegisteredComponent();
  VLOG(1) << "Registered VR assets component";
}

}  // namespace component_updater
