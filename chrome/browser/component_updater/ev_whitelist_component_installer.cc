// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/ev_whitelist_component_installer.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/ssl_config_service.h"

using component_updater::ComponentUpdateService;

namespace {
const base::FilePath::CharType kCompressedEVWhitelistFileName[] =
    FILE_PATH_LITERAL("ev_hashes_whitelist.bin");
}  // namespace

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: oafdbfcohdcjandcenmccfopbeklnicp
const uint8 kPublicKeySHA256[32] = {
    0xe0, 0x53, 0x15, 0x2e, 0x73, 0x29, 0x0d, 0x32, 0x4d, 0xc2, 0x25,
    0xef, 0x14, 0xab, 0xd8, 0x2f, 0x84, 0xf5, 0x85, 0x9e, 0xc0, 0xfa,
    0x94, 0xbc, 0x99, 0xc9, 0x5a, 0x27, 0x55, 0x19, 0x83, 0xef};

const char kEVWhitelistManifestName[] = "EV Certs CT whitelist";

EVWhitelistComponentInstallerTraits::EVWhitelistComponentInstallerTraits() {
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

  // TODO(eranm): Uncomment once https://codereview.chromium.org/462543002/
  // is in.
  /*
  const base::FilePath whitelist_file = GetInstalledPath(path);
  base::Callback<void(void)> set_cb =
      base::Bind(&net::ct::SetEVWhitelistFromFile, whitelist_file);
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      set_cb);
      */
}

bool EVWhitelistComponentInstallerTraits::VerifyInstallation(
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
    std::vector<uint8>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + arraysize(kPublicKeySHA256));
}

std::string EVWhitelistComponentInstallerTraits::GetName() const {
  return kEVWhitelistManifestName;
}

void RegisterEVWhitelistComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering EV whitelist component.";

  scoped_ptr<ComponentInstallerTraits> traits(
      new EVWhitelistComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus);
}

}  // namespace component_updater
