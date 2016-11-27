// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_
#define CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class HostContentSettingsMap;

// This class manages the Plugins field trials.
class PluginsFieldTrial {
 public:
  // The name of the variations parameter used for updating the amount of site
  // engagement required to permit Flash.
  static const char* kSiteEngagementThresholdForFlashKey;

  // Returns the effective content setting for plugins. Passes non-plugin
  // content settings through without modification.
  static ContentSetting EffectiveContentSetting(
      const HostContentSettingsMap* host_content_settings_map,
      ContentSettingsType type,
      ContentSetting setting);

  // Returns the engagement cutoff for permitting Flash to run without a prompt.
  static double GetSiteEngagementThresholdForFlash();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PluginsFieldTrial);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGINS_FIELD_TRIAL_H_
