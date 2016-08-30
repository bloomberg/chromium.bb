// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_filter_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/common/webplugininfo.h"
#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

namespace {

// For certain sandboxed Pepper plugins, use the JavaScript Content Settings.
bool ShouldUseJavaScriptSettingForPlugin(const content::WebPluginInfo& plugin) {
  if (plugin.type != content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type !=
          content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

#if !defined(DISABLE_NACL)
  // Treat Native Client invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(nacl::kNaClPluginName))
    return true;
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(kWidevineCdmDisplayName)) {
    DCHECK_EQ(content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
              plugin.type);
    return true;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)

  return false;
}

}  // namespace

void GetPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const content::WebPluginInfo& plugin,
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* uses_default_content_setting,
    bool* is_managed) {
  std::unique_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (ShouldUseJavaScriptSettingForPlugin(plugin)) {
    value = host_content_settings_map->GetWebsiteSetting(
        policy_url, policy_url, CONTENT_SETTINGS_TYPE_JAVASCRIPT, std::string(),
        &info);
  } else {
    content_settings::SettingInfo specific_info;
    std::unique_ptr<base::Value> specific_setting =
        host_content_settings_map->GetWebsiteSetting(
            policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource,
            &specific_info);
    content_settings::SettingInfo general_info;
    std::unique_ptr<base::Value> general_setting =
        host_content_settings_map->GetWebsiteSetting(
            policy_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS,
            std::string(), &general_info);
    // If there is a plugin-specific setting, we use it, unless the general
    // setting was set by policy, in which case it takes precedence.
    uses_plugin_specific_setting =
        specific_setting &&
        general_info.source != content_settings::SETTING_SOURCE_POLICY;
    if (uses_plugin_specific_setting) {
      value = std::move(specific_setting);
      info = specific_info;
    } else {
      value = std::move(general_setting);
      info = general_info;
    }
  }
  *setting = content_settings::ValueToContentSetting(value.get());
  if (uses_default_content_setting) {
    *uses_default_content_setting =
        !uses_plugin_specific_setting &&
        info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard();
  }
  if (is_managed)
    *is_managed = info.source == content_settings::SETTING_SOURCE_POLICY;
}
