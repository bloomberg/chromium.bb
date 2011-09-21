// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/content_settings_types.h"

namespace content_settings {

typedef std::pair<ContentSettingsPattern, ContentSettingsPattern> PatternPair;

// Returns true if the |content_type| requires a resource identifier.
bool RequiresResourceIdentifier(ContentSettingsType content_type);

// Maps CONTENT_SETTING_ASK for the CONTENT_SETTINGS_TYPE_PLUGINS to
// CONTENT_SETTING_BLOCK if click-to-play is not enabled.
ContentSetting ClickToPlayFixup(ContentSettingsType content_type,
                                ContentSetting setting);

// Converts |Value| to |ContentSetting|.
ContentSetting ValueToContentSetting(Value* value);

PatternPair ParsePatternString(const std::string& pattern_str);

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_
