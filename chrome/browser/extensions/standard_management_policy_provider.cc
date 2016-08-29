// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// Returns whether the extension can be modified under admin policy or not, and
// fills |error| with corresponding error message if necessary.
bool AdminPolicyIsModifiable(const extensions::Extension* extension,
                             base::string16* error) {
  if (!extensions::Manifest::IsComponentLocation(extension->location()) &&
      !extensions::Manifest::IsPolicyLocation(extension->location())) {
    return true;
  }

  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_MODIFY_POLICY_REQUIRED,
        base::UTF8ToUTF16(extension->name()));
  }

  return false;
}

bool ReturnLoadError(const extensions::Extension* extension,
                     base::string16* error) {
  if (error) {
    *error = l10n_util::GetStringFUTF16(
        IDS_EXTENSION_CANT_INSTALL_POLICY_BLOCKED,
        base::UTF8ToUTF16(extension->name()),
        base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace

StandardManagementPolicyProvider::StandardManagementPolicyProvider(
    const ExtensionManagement* settings)
    : settings_(settings) {
}

StandardManagementPolicyProvider::~StandardManagementPolicyProvider() {
}

std::string
    StandardManagementPolicyProvider::GetDebugPolicyProviderName() const {
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "extension management policy controlled settings";
#endif
}

bool StandardManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    base::string16* error) const {
  // Component extensions are always allowed.
  if (Manifest::IsComponentLocation(extension->location()))
    return true;

  // Shared modules are always allowed too: they only contain resources that
  // are used by other extensions. The extension that depends on the shared
  // module may be filtered by policy.
  if (extension->is_shared_module())
    return true;

  ExtensionManagement::InstallationMode installation_mode =
      settings_->GetInstallationMode(extension);

  // Force-installed extensions cannot be overwritten manually.
  if (!Manifest::IsPolicyLocation(extension->location()) &&
      installation_mode == ExtensionManagement::INSTALLATION_FORCED) {
    return ReturnLoadError(extension, error);
  }

  // Check whether the extension type is allowed.
  //
  // If you get a compile error here saying that the type you added is not
  // handled by the switch statement below, please consider whether enterprise
  // policy should be able to disallow extensions of the new type. If so, add
  // a branch to the second block and add a line to the definition of
  // kAllowedTypesMap in extension_management_constants.h.
  switch (extension->GetType()) {
    case Manifest::TYPE_UNKNOWN:
      break;
    case Manifest::TYPE_EXTENSION:
    case Manifest::TYPE_THEME:
    case Manifest::TYPE_USER_SCRIPT:
    case Manifest::TYPE_HOSTED_APP:
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
    case Manifest::TYPE_PLATFORM_APP:
    case Manifest::TYPE_SHARED_MODULE: {
      if (!settings_->IsAllowedManifestType(extension->GetType()))
        return ReturnLoadError(extension, error);
      break;
    }
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }

  if (installation_mode == ExtensionManagement::INSTALLATION_BLOCKED)
    return ReturnLoadError(extension, error);

  return true;
}

bool StandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    base::string16* error) const {
  return AdminPolicyIsModifiable(extension, error);
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    base::string16* error) const {
  return !AdminPolicyIsModifiable(extension, error);
}

bool StandardManagementPolicyProvider::MustRemainDisabled(
    const Extension* extension,
    Extension::DisableReason* reason,
    base::string16* error) const {
  std::string required_version;
  if (!settings_->CheckMinimumVersion(extension, &required_version)) {
    if (reason)
      *reason = Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY;
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_DISABLED_UPDATE_REQUIRED_BY_POLICY,
          base::UTF8ToUTF16(extension->name()),
          base::ASCIIToUTF16(required_version));
    }
    return true;
  }
  return false;
}

bool StandardManagementPolicyProvider::MustRemainInstalled(
    const Extension* extension,
    base::string16* error) const {
  ExtensionManagement::InstallationMode mode =
      settings_->GetInstallationMode(extension);
  // Disallow removing of recommended extension, to avoid re-install it
  // again while policy is reload. But disabling of recommended extension is
  // allowed.
  if (mode == ExtensionManagement::INSTALLATION_FORCED ||
      mode == ExtensionManagement::INSTALLATION_RECOMMENDED) {
    if (error) {
      *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_UNINSTALL_POLICY_REQUIRED,
          base::UTF8ToUTF16(extension->name()));
    }
    return true;
  }
  return false;
}

}  // namespace extensions
