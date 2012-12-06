// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/standard_management_policy_provider.h"

#include "chrome/browser/extensions/admin_policy.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"

namespace extensions {

StandardManagementPolicyProvider::StandardManagementPolicyProvider(
    ExtensionPrefs* prefs, Blacklist* blacklist)
    : prefs_(prefs), blacklist_(blacklist) {
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
  bool is_google_blacklisted = blacklist_->IsBlacklisted(extension);
  const base::ListValue* blacklist =
      prefs_->pref_service()->GetList(prefs::kExtensionInstallDenyList);
  const base::ListValue* whitelist =
      prefs_->pref_service()->GetList(prefs::kExtensionInstallAllowList);
  const base::ListValue* forcelist =
      prefs_->pref_service()->GetList(prefs::kExtensionInstallForceList);
  return admin_policy::UserMayLoad(is_google_blacklisted, blacklist, whitelist,
                                   forcelist, extension, error);
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
