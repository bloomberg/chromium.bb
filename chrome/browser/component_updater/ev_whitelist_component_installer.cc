// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ev_whitelist_component_installer.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/packed_ct_ev_whitelist/packed_ct_ev_whitelist.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/ssl_config_service.h"

using component_updater::ComponentUpdateService;

namespace {
const base::FilePath::CharType kCompressedEVWhitelistFileName[] =
    FILE_PATH_LITERAL("ev_hashes_whitelist.bin");

base::FilePath GetEVWhitelistFilePath(const base::FilePath& base_path) {
  return base_path.Append(kCompressedEVWhitelistFileName);
}

void UpdateNewWhitelistData(const base::FilePath& new_whitelist_file,
                            const base::FilePath& stored_whitelist_path,
                            const base::Version& version) {
  VLOG(1) << "Reading new EV whitelist from file: "
          << new_whitelist_file.value();
  std::string compressed_list;
  if (!base::ReadFileToString(new_whitelist_file, &compressed_list)) {
    VLOG(1) << "Failed reading from " << new_whitelist_file.value();
    return;
  }

  scoped_refptr<net::ct::EVCertsWhitelist> new_whitelist(
      new packed_ct_ev_whitelist::PackedEVCertsWhitelist(compressed_list,
                                                         version));
  if (!new_whitelist->IsValid()) {
    VLOG(1) << "Failed uncompressing EV certs whitelist.";
    return;
  }

  if (base::IsValueInRangeForNumericType<int>(compressed_list.size())) {
    const int list_size = base::checked_cast<int>(compressed_list.size());
    if (base::WriteFile(stored_whitelist_path, compressed_list.data(),
                        list_size) != list_size) {
      LOG(WARNING) << "Failed to save new EV whitelist to file.";
    }
  }

  packed_ct_ev_whitelist::SetEVCertsWhitelist(new_whitelist);
}

void DoInitialLoadFromDisk(const base::FilePath& stored_whitelist_path) {
  if (stored_whitelist_path.empty()) {
    return;
  }

  VLOG(1) << "Initial load: reading EV whitelist from file: "
          << stored_whitelist_path.value();
  std::string compressed_list;
  if (!base::ReadFileToString(stored_whitelist_path, &compressed_list)) {
    VLOG(1) << "Failed reading from " << stored_whitelist_path.value();
    return;
  }

  // The version number is unknown as the list is loaded from disk, not
  // the component.
  // In practice very quickly the component updater will call ComponentReady
  // which will have a valid version.
  scoped_refptr<net::ct::EVCertsWhitelist> new_whitelist(
      new packed_ct_ev_whitelist::PackedEVCertsWhitelist(compressed_list,
                                                         Version()));
  if (!new_whitelist->IsValid()) {
    VLOG(1) << "Failed uncompressing EV certs whitelist.";
    return;
  }

  VLOG(1) << "EV whitelist: Sucessfully loaded initial data.";
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

EVWhitelistComponentInstallerTraits::EVWhitelistComponentInstallerTraits(
    const base::FilePath& base_path)
    : ev_whitelist_path_(GetEVWhitelistFilePath(base_path)) {
}

bool EVWhitelistComponentInstallerTraits::CanAutoUpdate() const {
  return true;
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
    const base::FilePath& path,
    scoped_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << path.value();

  const base::FilePath whitelist_file = GetInstalledPath(path);
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&UpdateNewWhitelistData, whitelist_file,
                            ev_whitelist_path_, version));
}

bool EVWhitelistComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  const base::FilePath expected_file = GetInstalledPath(install_dir);
  VLOG(1) << "Verifying install: " << expected_file.value();
  if (!base::PathExists(expected_file)) {
    VLOG(1) << "File missing.";
    return false;
  }

  std::string compressed_whitelist;
  if (!base::ReadFileToString(expected_file, &compressed_whitelist)) {
    VLOG(1) << "Failed reading the compressed EV hashes whitelist.";
    return false;
  }

  VLOG(1) << "Whitelist size: " << compressed_whitelist.size();

  return !compressed_whitelist.empty();
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

void RegisterEVWhitelistComponent(ComponentUpdateService* cus,
                                  const base::FilePath& path) {
  VLOG(1) << "Registering EV whitelist component.";

  scoped_ptr<ComponentInstallerTraits> traits(
      new EVWhitelistComponentInstallerTraits(path));
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus, base::Closure());

  if (!content::BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&DoInitialLoadFromDisk, GetEVWhitelistFilePath(path)))) {
    NOTREACHED();
  }
}

}  // namespace component_updater
