// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/plugins_field_trial.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/plugins/common/plugins_switches.h"

namespace content_settings {

// static
const char PluginsFieldTrial::kFieldTrialName[] = "ForcePluginPowerSaver";

// static
ContentSetting PluginsFieldTrial::EffectiveContentSetting(
    ContentSettingsType type,
    ContentSetting setting) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return setting;

  // For Plugins, ASK is obsolete. Show as BLOCK to reflect actual behavior.
  if (setting == ContentSetting::CONTENT_SETTING_ASK)
    return ContentSetting::CONTENT_SETTING_BLOCK;

  return setting;
}

// static
bool PluginsFieldTrial::IsPluginPowerSaverEnabled() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(plugins::switches::kDisablePluginPowerSaver))
    return false;
  if (cl->HasSwitch(plugins::switches::kEnablePluginPowerSaver))
    return true;

  std::string group_name = base::FieldTrialList::FindFullName(kFieldTrialName);
  return !group_name.empty() && group_name != "Disabled";
}

// static
ContentSetting PluginsFieldTrial::GetDefaultPluginsContentSetting() {
  return IsPluginPowerSaverEnabled() ?
      ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT :
      ContentSetting::CONTENT_SETTING_ALLOW;
}

}  // namespace content_settings
