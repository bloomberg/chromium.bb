// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

// The extension id is: gcmjkmgdlgnkkcocmoeiminaijmmjnii
const uint8_t kPublicKeySHA256[32] = {
    0x62, 0xc9, 0xac, 0x63, 0xb6, 0xda, 0xa2, 0xe2, 0xce, 0x48, 0xc8,
    0xd0, 0x89, 0xcc, 0x9d, 0x88, 0x02, 0x7c, 0x3e, 0x71, 0xcf, 0x5d,
    0x6b, 0xb5, 0xdf, 0x21, 0x65, 0x82, 0x08, 0x97, 0x6a, 0x26};

const char kSubresourceFilterSetFetcherManifestName[] =
    "Subresource Filter Rules";

// static
const base::FilePath::CharType
    SubresourceFilterComponentInstallerTraits::kRulesetDataFileName[] =
        FILE_PATH_LITERAL("Filtering Rules");

// static
const base::FilePath::CharType
    SubresourceFilterComponentInstallerTraits::kLicenseFileName[] =
        FILE_PATH_LITERAL("LICENSE");

// static
const char
    SubresourceFilterComponentInstallerTraits::kManifestRulesetFormatKey[] =
        "ruleset_format";

// static
const int SubresourceFilterComponentInstallerTraits::kCurrentRulesetFormat = 1;

SubresourceFilterComponentInstallerTraits::
    SubresourceFilterComponentInstallerTraits() {}

SubresourceFilterComponentInstallerTraits::
    ~SubresourceFilterComponentInstallerTraits() {}

bool SubresourceFilterComponentInstallerTraits::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool SubresourceFilterComponentInstallerTraits::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
SubresourceFilterComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SubresourceFilterComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK(!install_dir.empty());
  DVLOG(1) << "Subresource Filter Version Ready: " << install_dir.value();
  int ruleset_format = 0;
  if (!manifest->GetInteger(kManifestRulesetFormatKey, &ruleset_format) ||
      ruleset_format != kCurrentRulesetFormat) {
    DVLOG(1) << "Bailing out. Future ruleset version: " << ruleset_format;
    return;
  }
  subresource_filter::UnindexedRulesetInfo ruleset_info;
  ruleset_info.content_version = version.GetString();
  ruleset_info.ruleset_path = install_dir.Append(kRulesetDataFileName);
  ruleset_info.license_path = install_dir.Append(kLicenseFileName);
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  if (ruleset_service)
    ruleset_service->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
}

// Called during startup and installation before ComponentReady().
bool SubresourceFilterComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir);
}

base::FilePath
SubresourceFilterComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(subresource_filter::kTopLevelDirectoryName)
      .Append(subresource_filter::kUnindexedRulesetBaseDirectoryName);
}

void SubresourceFilterComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string SubresourceFilterComponentInstallerTraits::GetName() const {
  return kSubresourceFilterSetFetcherManifestName;
}

// static
std::string SubresourceFilterComponentInstallerTraits::GetInstallerTag() {
  std::string ruleset_flavor = subresource_filter::GetRulesetFlavor();
  if (ruleset_flavor.empty())
    return ruleset_flavor;

  // We allow 4 ruleset flavor identifiers: a, b, c, d
  if (ruleset_flavor.size() == 1 && ruleset_flavor.at(0) >= 'a' &&
      ruleset_flavor.at(0) <= 'd')
    return ruleset_flavor;

  // Return 'invalid' for any cases where we encounter an invalid installer
  // tag. This allows us to verify that no clients are encountering invalid
  // installer tags in the field.
  return "invalid";
}

update_client::InstallerAttributes
SubresourceFilterComponentInstallerTraits::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  std::string installer_tag = GetInstallerTag();
  if (!installer_tag.empty())
    attributes["tag"] = installer_tag;
  return attributes;
}

std::vector<std::string>
SubresourceFilterComponentInstallerTraits::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterSubresourceFilterComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter))
    return;
  std::unique_ptr<ComponentInstallerTraits> traits(
      new SubresourceFilterComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
