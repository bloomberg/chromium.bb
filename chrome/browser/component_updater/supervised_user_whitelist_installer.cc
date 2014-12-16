// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/supervised_user_whitelist_installer.h"

#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/crx_file/id_util.h"

namespace component_updater {

namespace {

// See the corresponding entries in extensions::manifest_keys.
const char kContentPack[] = "content_pack";
const char kContentPackSites[] = "sites";

base::FilePath GetWhitelistPath(const base::DictionaryValue& manifest,
                                const base::FilePath& install_dir) {
  const base::DictionaryValue* content_pack_dict = nullptr;
  if (!manifest.GetDictionary(kContentPack, &content_pack_dict))
    return base::FilePath();

  base::FilePath::StringType whitelist_file;
  if (!content_pack_dict->GetString(kContentPackSites, &whitelist_file))
    return base::FilePath();

  return install_dir.Append(whitelist_file);
}

class SupervisedUserWhitelistComponentInstallerTraits
    : public ComponentInstallerTraits {
 public:
  SupervisedUserWhitelistComponentInstallerTraits(
      const std::string& crx_id,
      const std::string& name,
      const SupervisedUserWhitelistInstaller::WhitelistReadyCallback& callback)
      : crx_id_(crx_id), name_(name), callback_(callback) {}
  ~SupervisedUserWhitelistComponentInstallerTraits() override {}

 private:
  // ComponentInstallerTraits overrides:
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool CanAutoUpdate() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      scoped_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetBaseDirectory() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;

  std::string crx_id_;
  std::string name_;
  SupervisedUserWhitelistInstaller::WhitelistReadyCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserWhitelistComponentInstallerTraits);
};

bool SupervisedUserWhitelistComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Check whether the whitelist exists at the path specified by the manifest.
  // This does not check whether the whitelist is wellformed.
  return base::PathExists(GetWhitelistPath(manifest, install_dir));
}

bool SupervisedUserWhitelistComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool SupervisedUserWhitelistComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;
}

void SupervisedUserWhitelistComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    scoped_ptr<base::DictionaryValue> manifest) {
  callback_.Run(GetWhitelistPath(*manifest, install_dir));
}

base::FilePath
SupervisedUserWhitelistComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(DIR_SUPERVISED_USER_WHITELISTS, &result);
  return result;
}

void SupervisedUserWhitelistComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  *hash = SupervisedUserWhitelistInstaller::GetHashFromCrxId(crx_id_);
}

std::string SupervisedUserWhitelistComponentInstallerTraits::GetName() const {
  return name_;
}

class SupervisedUserWhitelistInstallerImpl
    : public SupervisedUserWhitelistInstaller {
 public:
  explicit SupervisedUserWhitelistInstallerImpl(ComponentUpdateService* cus);
  ~SupervisedUserWhitelistInstallerImpl() override {}

 private:
  // SupervisedUserWhitelistInstaller overrides:
  void RegisterWhitelist(const std::string& crx_id,
                         const std::string& name,
                         bool newly_added,
                         const WhitelistReadyCallback& callback) override;
  void UnregisterWhitelist(const std::string& crx_id) override;

  ComponentUpdateService* cus_;
};

SupervisedUserWhitelistInstallerImpl::SupervisedUserWhitelistInstallerImpl(
    ComponentUpdateService* cus)
    : cus_(cus) {
}

void SupervisedUserWhitelistInstallerImpl::RegisterWhitelist(
    const std::string& crx_id,
    const std::string& name,
    bool newly_added,
    const WhitelistReadyCallback& callback) {
  scoped_ptr<ComponentInstallerTraits> traits(
      new SupervisedUserWhitelistComponentInstallerTraits(crx_id, name,
                                                          callback));
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());

  // Takes ownership of |installer|.
  installer->Register(cus_);

  if (newly_added)
    TriggerComponentUpdate(&cus_->GetOnDemandUpdater(), crx_id);
}

void SupervisedUserWhitelistInstallerImpl::UnregisterWhitelist(
    const std::string& id) {
  // TODO(bauerb): Implement!
  NOTIMPLEMENTED();
}

}  // namespace

// static
scoped_ptr<SupervisedUserWhitelistInstaller>
SupervisedUserWhitelistInstaller::Create(ComponentUpdateService* cus) {
  return make_scoped_ptr(new SupervisedUserWhitelistInstallerImpl(cus));
}

// static
std::vector<uint8_t> SupervisedUserWhitelistInstaller::GetHashFromCrxId(
    const std::string& crx_id) {
  DCHECK(crx_file::id_util::IdIsValid(crx_id));

  std::vector<uint8_t> hash;
  uint8_t byte = 0;
  for (size_t i = 0; i < crx_id.size(); ++i) {
    // Uppercase characters in IDs are technically legal.
    int val = base::ToLowerASCII(crx_id[i]) - 'a';
    DCHECK_GE(val, 0);
    DCHECK_LT(val, 16);
    if (i % 2 == 0) {
      byte = val;
    } else {
      hash.push_back(16 * byte + val);
      byte = 0;
    }
  }
  return hash;
}

// static
void SupervisedUserWhitelistInstaller::TriggerComponentUpdate(
    OnDemandUpdater* updater,
    const std::string& crx_id) {
  updater->OnDemandUpdate(crx_id);
}

}  // namespace component_updater
