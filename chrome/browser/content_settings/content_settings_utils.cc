// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/common/chrome_switches.h"

namespace {

// True if a given content settings type requires additional resource
// identifiers.
const bool kRequiresResourceIdentifier[CONTENT_SETTINGS_NUM_TYPES] = {
  false,  // CONTENT_SETTINGS_TYPE_COOKIES
  false,  // CONTENT_SETTINGS_TYPE_IMAGES
  false,  // CONTENT_SETTINGS_TYPE_JAVASCRIPT
  true,   // CONTENT_SETTINGS_TYPE_PLUGINS
  false,  // CONTENT_SETTINGS_TYPE_POPUPS
  false,  // Not used for Geolocation
  false,  // Not used for Notifications
};

const char* kPatternSeparator = ",";

}  // namespace

namespace content_settings {

bool RequiresResourceIdentifier(ContentSettingsType content_type) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableResourceContentSettings)) {
    return kRequiresResourceIdentifier[content_type];
  } else {
    return false;
  }
}

ContentSetting ClickToPlayFixup(ContentSettingsType content_type,
                                ContentSetting setting) {
  if (setting == CONTENT_SETTING_ASK &&
      content_type == CONTENT_SETTINGS_TYPE_PLUGINS &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClickToPlay)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern) {
  return item_pattern.ToString()
         + std::string(kPatternSeparator)
         + top_level_frame_pattern.ToString();
}

PatternPair ParsePatternString(const std::string& pattern_str) {
  DCHECK(!pattern_str.empty());
  size_t pos = pattern_str.find(kPatternSeparator);

  std::pair<ContentSettingsPattern, ContentSettingsPattern> pattern_pair;
  if (pos == std::string::npos) {
    pattern_pair.first = ContentSettingsPattern::FromString(pattern_str);
    DCHECK(pattern_pair.first.IsValid());
    pattern_pair.second = ContentSettingsPattern();
  } else {
    pattern_pair.first = ContentSettingsPattern::FromString(
        pattern_str.substr(0, pos));
    DCHECK(pattern_pair.first.IsValid());
    pattern_pair.second = ContentSettingsPattern::FromString(
        pattern_str.substr(pos+1, pattern_str.size() - pos - 1));
    DCHECK(pattern_pair.second.IsValid());
  }
  return pattern_pair;
}

ContentSetting ValueToContentSetting(Value* value) {
  int int_value;
  value->GetAsInteger(&int_value);
  return IntToContentSetting(int_value);
}

}  // namespace content_settings
