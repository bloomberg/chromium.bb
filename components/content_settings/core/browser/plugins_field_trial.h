// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace content_settings {

// This class manages the Plugins field trials.
class PluginsFieldTrial {
 public:
  // A legacy field trial used to dogfood Plugin Power Saver. Remove soon.
  static const char kForceFieldTrial[];

  // The field trial responsible for rolling out Plugin Power Saver to users.
  static const char kEnableFieldTrial[];

  // Returns the effective content setting for plugins. Passes non-plugin
  // content settings through without modification.
  static ContentSetting EffectiveContentSetting(ContentSettingsType type,
                                                ContentSetting setting);

  // Returns true if the Plugin Power Saver feature is enabled.
  static bool IsPluginPowerSaverEnabled();

  // Get the default plugins content setting based on field trials/flags.
  static ContentSetting GetDefaultPluginsContentSetting();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginsFieldTrial);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_CONTENT_SETTINGS_H_
