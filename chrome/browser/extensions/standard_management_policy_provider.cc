// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/admin_policy.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

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
    string16* error) const {
  PrefService* pref_service = prefs_->pref_service();

  const base::ListValue* blacklist =
      pref_service->GetList(prefs::kExtensionInstallDenyList);
  const base::ListValue* whitelist =
      pref_service->GetList(prefs::kExtensionInstallAllowList);
  const base::DictionaryValue* forcelist =
      pref_service->GetDictionary(prefs::kExtensionInstallForceList);
  const base::ListValue* allowed_types = NULL;
  if (pref_service->HasPrefPath(prefs::kExtensionAllowedTypes))
    allowed_types = pref_service->GetList(prefs::kExtensionAllowedTypes);

  return admin_policy::UserMayLoad(
      blacklist, whitelist, forcelist, allowed_types, extension, error);
}

bool StandardManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    string16* error) const {
  return admin_policy::UserMayModifySettings(extension, error);
}

bool StandardManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    string16* error) const {
  return admin_policy::MustRemainEnabled(extension, error);
}

}  // namespace extensions
