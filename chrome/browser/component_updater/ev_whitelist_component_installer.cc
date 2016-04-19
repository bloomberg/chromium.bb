// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ev_whitelist_component_installer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/packed_ct_ev_whitelist/packed_ct_ev_whitelist.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {
const base::FilePath::CharType kCompressedEVWhitelistFileName[] =
    FILE_PATH_LITERAL("ev_hashes_whitelist.bin");

// Prior implementations (< M46) of this installer would copy the whitelist
// file from the installed component directory to the top of the user data
// directory which is not necessary. Delete this unused file.
// todo(cmumford): Delete this function >= M50.
void DeleteWhitelistCopy(const base::FilePath& user_data_dir) {
  base::DeleteFile(user_data_dir.Append(kCompressedEVWhitelistFileName), false);
}

void LoadWhitelistFromDisk(const base::FilePath& whitelist_path,
                           const base::Version& version) {
  if (whitelist_path.empty())
    return;

  VLOG(1) << "Reading EV whitelist from file: " << whitelist_path.value();
  std::string compressed_list;
  if (!base::ReadFileToString(whitelist_path, &compressed_list)) {
    VLOG(1) << "Failed reading from " << whitelist_path.value();
    return;
  }

  scoped_refptr<net::ct::EVCertsWhitelist> new_whitelist(
      new packed_ct_ev_whitelist::PackedEVCertsWhitelist(compressed_list,
                                                         version));
  compressed_list.clear();
  if (!new_whitelist->IsValid()) {
    VLOG(1) << "Failed uncompressing EV certs whitelist.";
    return;
  }

  VLOG(1) << "EV whitelist: Successfully loaded.";
  packed_ct_ev_whitelist::SetEVCertsWhitelist(new_whitelist);
}

}  // namespace

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: oafdbfcohdcjandcenmccfopbeklnicp
const uint8_t kPublicKeySHA256[32] = {
    0xe0, 0x53, 0x15, 0x2e, 0x73, 0x29, 0x0d, 0x32, 0x4d, 0xc2, 0x25,
    0xef, 0x14, 0xab, 0xd8, 0x2f, 0x84, 0xf5, 0x85, 0x9e, 0xc0, 0xfa,
    0x94, 0xbc, 0x99, 0xc9, 0x5a, 0x27, 0x55, 0x19, 0x83, 0xef};

const char kEVWhitelistManifestName[] = "EV Certs CT whitelist";

bool EVWhitelistComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool EVWhitelistComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

bool EVWhitelistComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  VLOG(1) << "Entering EVWhitelistComponentInstallerTraits::OnCustomInstall.";

  return true;  // Nothing custom here.
}

base::FilePath EVWhitelistComponentInstallerTraits::GetInstalledPath(
    const base::FilePath& base) {
  // EV whitelist is encoded the same way for all platforms
  return base.Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(kCompressedEVWhitelistFileName);
}

void EVWhitelistComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  if (!content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE, base::Bind(&LoadWhitelistFromDisk,
                                GetInstalledPath(install_dir), version))) {
    NOTREACHED();
  }
}

// Called during startup and installation before ComponentReady().
bool EVWhitelistComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath EVWhitelistComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(DIR_COMPONENT_EV_WHITELIST, &result);
  return result;
}

void EVWhitelistComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + arraysize(kPublicKeySHA256));
}

std::string EVWhitelistComponentInstallerTraits::GetName() const {
  return kEVWhitelistManifestName;
}

std::string EVWhitelistComponentInstallerTraits::GetAp() const {
  return std::string();
}

void RegisterEVWhitelistComponent(ComponentUpdateService* cus,
                                  const base::FilePath& user_data_dir) {
  VLOG(1) << "Registering EV whitelist component.";

  std::unique_ptr<ComponentInstallerTraits> traits(
      new EVWhitelistComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());

  content::BrowserThread::PostAfterStartupTask(
      FROM_HERE, content::BrowserThread::GetBlockingPool(),
      base::Bind(&DeleteWhitelistCopy, user_data_dir));
}

}  // namespace component_updater
