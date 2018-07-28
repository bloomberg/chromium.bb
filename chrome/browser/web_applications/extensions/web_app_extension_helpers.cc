// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_helpers.h"

#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace web_app {

std::string GenerateApplicationNameFromExtensionId(const std::string& id) {
  return GenerateApplicationNameFromAppId(id);
}

std::string GetExtensionIdFromApplicationName(const std::string& app_name) {
  return GetAppIdFromApplicationName(app_name);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kWebAppInstallForceList);
}

}  // namespace web_app
