// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/external_component_loader.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/admin_policy.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"

namespace extensions {

StandardManagementPolicyProvider::StandardManagementPolicyProvider(
    ExtensionPrefs* prefs)
    : prefs_(prefs) {
}

StandardManagementPolicyProvider::~StandardManagementPolicyProvider() {
}

std::string
    StandardManagementPolicyProvider::GetDebugPolicyProviderName() const {
#ifdef NDEBUG
  NOTREACHED();
  return std::string();
#else
  return "admin policy black/white/forcelist, via the ExtensionPrefs";
#endif
}

bool StandardManagementPolicyProvider::UserMayLoad(
    const Extension* extension,
    base::string16* error) const {
  PrefService* pref_service = prefs_->pref_service();

  const base::ListValue* blacklist =
      pref_service->GetList(pref_names::kInstallDenyList);
  const base::ListValue* whitelist =
      pref_service->GetList(pref_names::kInstallAllowList);
  const base::DictionaryValue* forcelist =
      pref_service->GetDictionary(pref_names::kInstallForceList);
  const base::ListValue* allowed_types = NULL;
  if (pref_service->HasPrefPath(pref_names::kAllowedTypes))
    allowed_types = pref_service->GetList(pref_names::kAllowedTypes);

  return admin_policy::UserMayLoad(
      blacklist, whitelist, forcelist, allowed_types, extension, error);
}

bool StandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    base::string16* error) const {
  return admin_policy::UserMayModifySettings(extension, error) ||
         (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT &&
          ExternalComponentLoader::IsModifiable(extension));
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    base::string16* error) const {
  return admin_policy::MustRemainEnabled(extension, error) ||
         (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT &&
          ExternalComponentLoader::IsModifiable(extension));
}

}  // namespace extensions
