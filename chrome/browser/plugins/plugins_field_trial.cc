// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_field_trial.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"

// static
ContentSetting PluginsFieldTrial::EffectiveContentSetting(
    ContentSettingsType type,
    ContentSetting setting) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS ||
      setting != ContentSetting::CONTENT_SETTING_ASK) {
    return setting;
  }

  // For Plugins, ASK is obsolete. Show as BLOCK or, if PreferHtmlOverPlugins
  // feature is enabled, as DETECT_IMPORTANT_CONTENT to reflect actual behavior.
  return base::FeatureList::IsEnabled(features::kPreferHtmlOverPlugins)
             ? ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT
             : ContentSetting::CONTENT_SETTING_BLOCK;
}
