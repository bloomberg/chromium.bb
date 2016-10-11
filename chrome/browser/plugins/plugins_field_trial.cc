// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_field_trial.h"

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/variations/variations_associated_data.h"

namespace {

// The default site engagement threshold to allow Flash to be presented as an
// available plugin. This value is not used for normal uses, but only for
// developers / testers that don't have a field trial configuration.
const double kDefaultSiteEngagementThresholdForFlash = 30.0;

}  // namespace

// static
const char* PluginsFieldTrial::kSiteEngagementThresholdForFlashKey =
    "engagement_threshold_for_flash";

// static
ContentSetting PluginsFieldTrial::EffectiveContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    ContentSettingsType type,
    ContentSetting setting) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS ||
      setting != ContentSetting::CONTENT_SETTING_ASK) {
    return setting;
  }

  // For Plugins, ASK is obsolete. Show as BLOCK or, if PreferHtmlOverPlugins
  // feature is enabled, as DETECT_IMPORTANT_CONTENT to reflect actual behavior.
  return PluginUtils::ShouldPreferHtmlOverPlugins(host_content_settings_map)
             ? ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT
             : ContentSetting::CONTENT_SETTING_BLOCK;
}

// static
double PluginsFieldTrial::GetSiteEngagementThresholdForFlash() {
  double threshold = -1;
  std::string param = variations::GetVariationParamValueByFeature(
      features::kPreferHtmlOverPlugins,
      PluginsFieldTrial::kSiteEngagementThresholdForFlashKey);
  if (base::StringToDouble(param, &threshold) && threshold >= 0)
    return threshold;
  return kDefaultSiteEngagementThresholdForFlash;
}
