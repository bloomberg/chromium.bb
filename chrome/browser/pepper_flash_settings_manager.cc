// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pepper_flash_settings_manager.h"

#include <vector>

#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/plugin_service.h"
#include "googleurl/src/gurl.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/webplugininfo.h"

// static
bool PepperFlashSettingsManager::IsPepperFlashInUse(
    PluginPrefs* plugin_prefs,
    webkit::WebPluginInfo* plugin_info) {
  if (!plugin_prefs)
    return false;

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();
  std::vector<webkit::WebPluginInfo> plugins;
  plugin_service->GetPluginInfoArray(
      GURL(), kFlashPluginSwfMimeType, false, &plugins, NULL);

  for (std::vector<webkit::WebPluginInfo>::iterator iter = plugins.begin();
       iter != plugins.end(); ++iter) {
    if (webkit::IsPepperPlugin(*iter) && plugin_prefs->IsPluginEnabled(*iter)) {
      if (plugin_info)
        *plugin_info = *iter;
      return true;
    }
  }
  return false;
}

// static
void PepperFlashSettingsManager::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDeauthorizeContentLicenses,
                             false,
                             PrefService::UNSYNCABLE_PREF);

  prefs->RegisterBooleanPref(prefs::kPepperFlashSettingsEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}
