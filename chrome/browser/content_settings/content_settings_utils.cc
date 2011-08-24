// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_utils.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/string_split.h"
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
  false,  // Not used for Intents.
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
  std::vector<std::string> pattern_str_list;
  base::SplitString(pattern_str, kPatternSeparator[0], &pattern_str_list);

  // If the |pattern_str| is an empty string then the |pattern_string_list|
  // contains a single empty string. In this case the empty string will be
  // removed to signal an invalid |pattern_str|. Invalid pattern strings are
  // handle by the "if"-statment below. So the order of the if statements here
  // must be preserved.
  if (pattern_str_list.size() == 1) {
    if (pattern_str_list[0].empty()) {
      pattern_str_list.pop_back();
    } else {
      pattern_str_list.push_back("*");
    }
  }

  if (pattern_str_list.size() > 2 ||
      pattern_str_list.size() == 0) {
    return PatternPair(ContentSettingsPattern(),
                       ContentSettingsPattern());
  }

  PatternPair pattern_pair;
  pattern_pair.first =
      ContentSettingsPattern::FromString(pattern_str_list[0]);
  pattern_pair.second =
      ContentSettingsPattern::FromString(pattern_str_list[1]);
  return pattern_pair;
}

ContentSetting ValueToContentSetting(Value* value) {
  int int_value;
  value->GetAsInteger(&int_value);
  return IntToContentSetting(int_value);
}

}  // namespace content_settings
