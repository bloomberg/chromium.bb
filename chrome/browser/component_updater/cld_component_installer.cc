// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cld_component_installer.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"
#include "components/translate/content/common/cld_data_source.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/ssl_config_service.h"

using component_updater::ComponentUpdateService;

namespace {
// TODO(andrewhayden): Make the data file path into a gyp/gn define
// If you change this, also update component_cld_data_harness.cc
// and cld_component_installer_unittest.cc accordingly!
const base::FilePath::CharType kCldDataFileName[] =
  FILE_PATH_LITERAL("cld2_data.bin");

// Tracks the last value seen in SetLatestCldDataFile.
base::LazyInstance<base::FilePath>::Leaky g_latest_cld_data_file =
  LAZY_INSTANCE_INITIALIZER;
}  // namespace

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: dpedmmgabcgnikllifiidmijgoiihfgf
const uint8_t kPublicKeySHA256[32] = {
    0x3f, 0x43, 0xcc, 0x60, 0x12, 0x6d, 0x8a, 0xbb,
    0x85, 0x88, 0x3c, 0x89, 0x6e, 0x88, 0x75, 0x65,
    0xb9, 0x46, 0x09, 0xe8, 0xca, 0x92, 0xdd, 0x82,
    0x4e, 0x6d, 0x0e, 0xe6, 0x79, 0x8a, 0x87, 0xf5
};

const char kCldManifestName[] = "CLD2 Data";

CldComponentInstallerTraits::CldComponentInstallerTraits() {
}

bool CldComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool CldComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;  // Nothing custom here.
}

base::FilePath CldComponentInstallerTraits::GetInstalledPath(
    const base::FilePath& base) {
  // Currently, all platforms have the file at the same location because there
  // is no binary difference in the generated file on any supported platform.
  // NB: This may change when 64-bit is officially supported.
  return base.Append(FILE_PATH_LITERAL("_platform_specific"))
      .Append(FILE_PATH_LITERAL("all"))
      .Append(kCldDataFileName);
}

void CldComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    scoped_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << path.value();
  SetLatestCldDataFile(GetInstalledPath(path));
}

bool CldComponentInstallerTraits::VerifyInstallation(
    const base::FilePath& install_dir) const {
  // We can't really do much to verify the CLD2 data file. In theory we could
  // read the headers, but that won't do much other than tell us whether or
  // not the headers are valid. So just check if the file exists.
  const base::FilePath expected_file = GetInstalledPath(install_dir);
  VLOG(1) << "Verifying install: " << expected_file.value();
  const bool result = base::PathExists(expected_file);
  VLOG(1) << "Verification result: " << (result ? "valid" : "invalid");
  return result;
}

base::FilePath CldComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(DIR_COMPONENT_CLD2, &result);
  return result;
}

void CldComponentInstallerTraits::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kPublicKeySHA256,
               kPublicKeySHA256 + arraysize(kPublicKeySHA256));
}

std::string CldComponentInstallerTraits::GetName() const {
  return kCldManifestName;
}

void RegisterCldComponent(ComponentUpdateService* cus) {
  // Make sure we don't start up if the CLD data source isn't compatible.
  if (!translate::CldDataSource::ShouldRegisterForComponentUpdates()) {
    // This is a serious build-time configuration error.
    LOG(ERROR) << "Wrong CLD data source: " <<
        translate::CldDataSource::GetName();
    NOTREACHED();
    return;
  }

  // This log line is to help with determining which kind of provider has been
  // configured. See also: chrome://translate-internals
  VLOG(1) << "Registering CLD component with the component update service";

  scoped_ptr<ComponentInstallerTraits> traits(
      new CldComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus);
}

void CldComponentInstallerTraits::SetLatestCldDataFile(
    const base::FilePath& path) {
  VLOG(1) << "Setting CLD data file location: " << path.value();
  g_latest_cld_data_file.Get() = path;
  translate::SetCldDataFilePath(path);
}

base::FilePath CldComponentInstallerTraits::GetLatestCldDataFile() {
  return g_latest_cld_data_file.Get();
}

}  // namespace component_updater
