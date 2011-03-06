// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installation_state.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"

namespace installer {

ProductState::ProductState()
    : uninstall_command_(CommandLine::NO_PROGRAM),
      msi_(false),
      multi_install_(false) {
}

bool ProductState::Initialize(bool system_install,
                              BrowserDistribution::Type type) {
  return Initialize(system_install,
                    BrowserDistribution::GetSpecificDistribution(type));
}

// Initializes |commands| from the "Commands" subkey of |version_key|.
// Returns false if there is no "Commands" subkey or on error.
// static
bool ProductState::InitializeCommands(const base::win::RegKey& version_key,
                                      AppCommands* commands) {
  static const DWORD kAccess = KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE;
  base::win::RegKey commands_key;

  if (!version_key.Valid()) {
    // Version key may be invalid if we're not running under Google Update.
    LOG(WARNING) << "Could not InitializeCommands, version_key is invalid.";
    return false;
  }

  if (commands_key.Open(version_key.Handle(), google_update::kRegCommandsKey,
                        kAccess) == ERROR_SUCCESS)
    return commands->Initialize(commands_key);
  return false;
}

bool ProductState::Initialize(bool system_install,
                              BrowserDistribution* distribution) {
  const std::wstring version_key(distribution->GetVersionKey());
  const std::wstring state_key(distribution->GetStateKey());
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key(root_key, version_key.c_str(), KEY_QUERY_VALUE);
  std::wstring version_str;
  if (key.ReadValue(google_update::kRegVersionField,
                    &version_str) == ERROR_SUCCESS) {
    version_.reset(Version::GetVersionFromString(WideToASCII(version_str)));
  }

  // Attempt to read the other values even if the "pv" version value was absent.
  // Note that ProductState instances containing these values will only be
  // accessible via InstallationState::GetNonVersionedProductState.
  if (key.ReadValue(google_update::kRegOldVersionField,
                    &version_str) == ERROR_SUCCESS) {
    old_version_.reset(
        Version::GetVersionFromString(WideToASCII(version_str)));
  }

  if (key.ReadValue(google_update::kRegRenameCmdField,
                    &rename_cmd_) != ERROR_SUCCESS)
    rename_cmd_.clear();

  if (!InitializeCommands(key, &commands_))
    commands_.Clear();

  // Read from the ClientState key.
  channel_.set_value(std::wstring());
  uninstall_command_ = CommandLine(CommandLine::NO_PROGRAM);
  brand_.clear();
  msi_ = false;
  multi_install_ = false;
  if (key.Open(root_key, state_key.c_str(),
               KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    std::wstring setup_path;
    std::wstring uninstall_arguments;
    // "ap" will be absent if not managed by Google Update.
    channel_.Initialize(key);

    // Read in the brand code, it may be absent
    key.ReadValue(google_update::kRegBrandField, &brand_);

    // "UninstallString" will be absent for the multi-installer package.
    key.ReadValue(kUninstallStringField, &setup_path);
    // "UninstallArguments" will be absent for the multi-installer package.
    key.ReadValue(kUninstallArgumentsField, &uninstall_arguments);
    InstallUtil::MakeUninstallCommand(setup_path, uninstall_arguments,
                                      &uninstall_command_);

    // "msi" may be absent, 0 or 1
    DWORD dw_value = 0;
    msi_ = (key.ReadValueDW(google_update::kRegMSIField,
                            &dw_value) == ERROR_SUCCESS) && (dw_value != 0);
    // Multi-install is implied or is derived from the command-line.
    if (distribution->GetType() == BrowserDistribution::CHROME_BINARIES) {
      multi_install_ = true;
    } else {
      multi_install_ = uninstall_command_.HasSwitch(
          switches::kMultiInstall);
    }
  }

  return version_.get() != NULL;
}

FilePath ProductState::GetSetupPath() const {
  return uninstall_command_.GetProgram();
}

const Version& ProductState::version() const {
  DCHECK(version_.get() != NULL);
  return *version_;
}

ProductState& ProductState::CopyFrom(const ProductState& other) {
  channel_.set_value(other.channel_.value());
  version_.reset(other.version_.get() == NULL ? NULL : other.version_->Clone());
  old_version_.reset(
      other.old_version_.get() == NULL ? NULL : other.old_version_->Clone());
  brand_ = other.brand_;
  rename_cmd_ = other.rename_cmd_;
  uninstall_command_ = other.uninstall_command_;
  commands_.CopyFrom(other.commands_);
  msi_ = other.msi_;
  multi_install_ = other.multi_install_;

  return *this;
}

InstallationState::InstallationState() {
}

// static
int InstallationState::IndexFromDistType(BrowserDistribution::Type type) {
  COMPILE_ASSERT(BrowserDistribution::CHROME_BROWSER == CHROME_BROWSER_INDEX,
                 unexpected_chrome_browser_distribution_value_);
  COMPILE_ASSERT(BrowserDistribution::CHROME_FRAME == CHROME_FRAME_INDEX,
                 unexpected_chrome_frame_distribution_value_);
  COMPILE_ASSERT(BrowserDistribution::CHROME_BINARIES == CHROME_BINARIES_INDEX,
                 unexpected_chrome_frame_distribution_value_);
  DCHECK(type == BrowserDistribution::CHROME_BROWSER ||
         type == BrowserDistribution::CHROME_FRAME ||
         type == BrowserDistribution::CHROME_BINARIES);
  return type;
}

void InstallationState::Initialize() {
  BrowserDistribution* distribution;

  distribution = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER);
  user_products_[CHROME_BROWSER_INDEX].Initialize(false, distribution);
  system_products_[CHROME_BROWSER_INDEX].Initialize(true, distribution);

  distribution = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME);
  user_products_[CHROME_FRAME_INDEX].Initialize(false, distribution);
  system_products_[CHROME_FRAME_INDEX].Initialize(true, distribution);

  distribution = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  user_products_[CHROME_BINARIES_INDEX].Initialize(false, distribution);
  system_products_[CHROME_BINARIES_INDEX].Initialize(true, distribution);
}

const ProductState* InstallationState::GetNonVersionedProductState(
    bool system_install,
    BrowserDistribution::Type type) const {
  const ProductState& product_state = (system_install ? system_products_ :
      user_products_)[IndexFromDistType(type)];
  return &product_state;
}

const ProductState* InstallationState::GetProductState(
    bool system_install,
    BrowserDistribution::Type type) const {
  const ProductState* product_state =
      GetNonVersionedProductState(system_install, type);
  return product_state->version_.get() == NULL ? NULL : product_state;
}
}  // namespace installer
