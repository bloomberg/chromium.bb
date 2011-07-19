// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_provider.h"

namespace content_settings {

ProviderInterface::Rule::Rule()
    : content_setting(CONTENT_SETTING_DEFAULT) {
}

ProviderInterface::Rule::Rule(const ContentSettingsPattern& primary_pattern,
                              const ContentSettingsPattern& secondary_pattern,
                              ContentSetting setting)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      content_setting(setting) {
}

}  // namespace content_settings
