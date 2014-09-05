// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_component_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "grit/extensions_strings.h"
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

  // Fields in |by_id| will automatically fall back to default settings if
  // they are not specified by policy.
  const ExtensionManagement::IndividualSettings& by_id =
      settings_->ReadById(extension->id());
  const ExtensionManagement::GlobalSettings& global =
      settings_->ReadGlobalSettings();

  // Force-installed extensions cannot be overwritten manually.
  if (!Manifest::IsPolicyLocation(extension->location()) &&
      by_id.installation_mode == ExtensionManagement::INSTALLATION_FORCED) {
    return ReturnLoadError(extension, error);
  }

  // Check whether the extension type is allowed.
  //
  // If you get a compile error here saying that the type you added is not
  // handled by the switch statement below, please consider whether enterprise
  // policy should be able to disallow extensions of the new type. If so, add
  // a branch to the second block and add a line to the definition of
  // kExtensionAllowedTypesMap in configuration_policy_handler_list.cc.
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
      if (global.has_restricted_allowed_types &&
          std::find(global.allowed_types.begin(),
                    global.allowed_types.end(),
                    extension->GetType()) == global.allowed_types.end()) {
        return ReturnLoadError(extension, error);
      }
      break;
    }
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }

  if (by_id.installation_mode == ExtensionManagement::INSTALLATION_BLOCKED)
    return ReturnLoadError(extension, error);

  return true;
}

bool StandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    base::string16* error) const {
  return AdminPolicyIsModifiable(extension, error) ||
         (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT &&
          ExternalComponentLoader::IsModifiable(extension));
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    base::string16* error) const {
  return !AdminPolicyIsModifiable(extension, error) ||
         (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT &&
          ExternalComponentLoader::IsModifiable(extension));
}

}  // namespace extensions
