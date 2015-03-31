// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugins_field_trial.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"

ContentSetting PluginsFieldTrial::EffectiveContentSetting(
    ContentSettingsType type,
    ContentSetting setting) {
  if (type != CONTENT_SETTINGS_TYPE_PLUGINS)
    return setting;

  // For Plugins, ASK is obsolete. Show as BLOCK to reflect actual behavior.
  if (setting == ContentSetting::CONTENT_SETTING_ASK)
    return ContentSetting::CONTENT_SETTING_BLOCK;

  // For Plugins, allow flag to override displayed content setting.
  if (setting == ContentSetting::CONTENT_SETTING_ALLOW &&
      IsForcePluginPowerSaverEnabled()) {
    return ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
  }

  return setting;
}

bool PluginsFieldTrial::IsForcePluginPowerSaverEnabled() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kDisablePluginPowerSaver))
    return false;
  if (cl->HasSwitch(switches::kEnablePluginPowerSaver))
    return true;

  std::string group_name =
      base::FieldTrialList::FindFullName("ForcePluginPowerSaver");
  return !group_name.empty() && group_name != "Disabled";
}
