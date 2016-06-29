// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

void PassDataToSubresourceFilterService(const std::string& rules,
                                        const base::Version& version) {
  if (g_browser_process->subresource_filter_ruleset_service()) {
    g_browser_process->subresource_filter_ruleset_service()
        ->NotifyRulesetVersionAvailable(rules, version);
  }
}

}  // namespace

namespace component_updater {

// The extension id is: gcmjkmgdlgnkkcocmoeiminaijmmjnii
const uint8_t kPublicKeySHA256[32] = {
    0x62, 0xc9, 0xac, 0x63, 0xb6, 0xda, 0xa2, 0xe2, 0xce, 0x48, 0xc8,
    0xd0, 0x89, 0xcc, 0x9d, 0x88, 0x02, 0x7c, 0x3e, 0x71, 0xcf, 0x5d,
    0x6b, 0xb5, 0xdf, 0x21, 0x65, 0x82, 0x08, 0x97, 0x6a, 0x26};

const char kSubresourceFilterSetFetcherManifestName[] =
    "Subresource Filter Rules";

SubresourceFilterComponentInstallerTraits::
    SubresourceFilterComponentInstallerTraits() {}

SubresourceFilterComponentInstallerTraits::
    ~SubresourceFilterComponentInstallerTraits() {}

bool SubresourceFilterComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

// Public data is delivered via this component, no need for encryption.
bool SubresourceFilterComponentInstallerTraits::RequiresNetworkEncryption()
    const {
  return false;
}

bool SubresourceFilterComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;  // Nothing custom here.
}

void SubresourceFilterComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&SubresourceFilterComponentInstallerTraits::
                                LoadSubresourceFilterRulesFromDisk,
                            base::Unretained(this), install_dir, version));
}

// Called during startup and installation before ComponentReady().
bool SubresourceFilterComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir);
}

base::FilePath
SubresourceFilterComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("SubresourceFilterRules"));
}

void SubresourceFilterComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string SubresourceFilterComponentInstallerTraits::GetName() const {
  return kSubresourceFilterSetFetcherManifestName;
}

update_client::InstallerAttributes
SubresourceFilterComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void SubresourceFilterComponentInstallerTraits::
    LoadSubresourceFilterRulesFromDisk(
        const base::FilePath& subresource_filter_path,
        const base::Version& version) {
  if (subresource_filter_path.empty())
    return;
  DVLOG(1) << "Subresource Filter: Successfully read: "
           << subresource_filter_path.value() << " " << version;
  std::string rules;
  base::FilePath full_path = subresource_filter_path.Append(
      FILE_PATH_LITERAL("subresource_filter_rules.blob"));
  if (!base::ReadFileToString(full_path, &rules)) {
    VLOG(1) << "Failed reading from " << full_path.value();
    return;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(PassDataToSubresourceFilterService, rules, version));
}

void RegisterSubresourceFilterComponent(ComponentUpdateService* cus) {
  std::unique_ptr<ComponentInstallerTraits> traits(
      new SubresourceFilterComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
