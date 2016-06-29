// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/origin_trials_component_installer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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
// TODO(iclelland): Implement support for revoked tokens.
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
  // Read the public key from the manifest and set values in browser
  // local_state. These will be used on the next browser restart.
  PrefService* local_state = g_browser_process->local_state();
  std::string override_public_key;
  if (manifest->GetString("origin-trials.public-key", &override_public_key)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::StringValue(override_public_key));
  }
  base::ListValue* override_disabled_feature_list = nullptr;
  if (manifest->GetList("origin-trials.disabled-features",
                        &override_disabled_feature_list)) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledFeatures);
    update->Swap(override_disabled_feature_list);
  }
}

base::FilePath OriginTrialsComponentInstallerTraits::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("OriginTrials"));
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

update_client::InstallerAttributes
OriginTrialsComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
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
