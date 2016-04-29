// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/origin_trials_component_installer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_switches.h"
#include "components/component_updater/component_updater_paths.h"

// The client-side configuration for the origin trial framework can be
// overridden by an installed component named 'OriginTrials' (extension id
// kfoklmclfodeliojeaekpoflbkkhojea. This component currently consists of just a
// manifest.json file, which can contain a custom key named 'origin-trials'. The
// value of this key is a dictionary:
//
// {
//   "public-key": "<base64-encoding of replacement public key>",
//   "disabled-features": [<list of features to disable>],
//   "revoked-tokens": "<base64-encoded data>"
// }
//
// TODO(iclelland): Implement support for revoked tokens and disabled features.
//
// If the component is not present in the user data directory, the default
// configuration will be used.

namespace component_updater {

namespace {

// Extension id is kfoklmclfodeliojeaekpoflbkkhojea
const uint8_t kSha256Hash[] = {0xa5, 0xea, 0xbc, 0x2b, 0x5e, 0x34, 0xb8, 0xe9,
                               0x40, 0x4a, 0xfe, 0x5b, 0x1a, 0xa7, 0xe9, 0x40,
                               0xa8, 0xc5, 0xef, 0xa1, 0x9e, 0x20, 0x5a, 0x39,
                               0x73, 0x98, 0x98, 0x0f, 0x7a, 0x76, 0x62, 0xfa};

}  // namespace

bool OriginTrialsComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Test if the "origin-trials" key is present in the manifest.
  return manifest.HasKey("origin-trials");
}

bool OriginTrialsComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool OriginTrialsComponentInstallerTraits::RequiresNetworkEncryption() const {
  return true;
}

bool OriginTrialsComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;
}

void OriginTrialsComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // Read the public key from the manifest and set the command line.
  std::string override_public_key;
  if (manifest->GetString("origin-trials.public-key", &override_public_key)) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kOriginTrialPublicKey,
                                    override_public_key);
  }
}

base::FilePath OriginTrialsComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(DIR_ORIGIN_TRIAL_KEYS, &result);
  return result;
}

void OriginTrialsComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  if (!hash)
    return;
  hash->assign(kSha256Hash, kSha256Hash + arraysize(kSha256Hash));
}

std::string OriginTrialsComponentInstallerTraits::GetName() const {
  return "Origin Trials";
}

std::string OriginTrialsComponentInstallerTraits::GetAp() const {
  return std::string();
}

void RegisterOriginTrialsComponent(ComponentUpdateService* cus,
                                   const base::FilePath& user_data_dir) {
  std::unique_ptr<ComponentInstallerTraits> traits(
      new OriginTrialsComponentInstallerTraits());
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
}

}  // namespace component_updater
