// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/plugins_field_trial.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/plugins/common/plugins_switches.h"

using base::FieldTrialList;

namespace content_settings {

// static
const char PluginsFieldTrial::kForceFieldTrial[] = "ForcePluginPowerSaver";

// static
const char PluginsFieldTrial::kEnableFieldTrial[] = "PluginPowerSaver";

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
  std::string enable_group = FieldTrialList::FindFullName(kEnableFieldTrial);
  std::string force_group = FieldTrialList::FindFullName(kForceFieldTrial);

  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(plugins::switches::kDisablePluginPowerSaver))
    return false;
  if (cl->HasSwitch(plugins::switches::kEnablePluginPowerSaver))
    return true;

  if (!enable_group.empty() && enable_group != "Disabled")
    return true;
  if (!force_group.empty() && force_group != "Disabled")
    return true;

  return false;
}

// static
ContentSetting PluginsFieldTrial::GetDefaultPluginsContentSetting() {
  return IsPluginPowerSaverEnabled() ?
      ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT :
      ContentSetting::CONTENT_SETTING_ALLOW;
}

}  // namespace content_settings
