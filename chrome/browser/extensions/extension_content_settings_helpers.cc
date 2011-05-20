// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_helpers.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace {

const char* const kContentSettingsTypeNames[] = {
  "cookies",
  "images",
  "javascript",
  "plugins",
  "popups",
  "location",
  "notifications",
};
COMPILE_ASSERT(arraysize(kContentSettingsTypeNames) <=
               CONTENT_SETTINGS_NUM_TYPES,
               content_settings_type_names_size_invalid);

const char* const kContentSettingNames[] = {
  "default",
  "allow",
  "block",
  "ask",
  "session_only",
};
COMPILE_ASSERT(arraysize(kContentSettingNames) <=
               CONTENT_SETTING_NUM_SETTINGS,
               content_setting_names_size_invalid);

}  // namespace

namespace extension_content_settings_helpers {

ContentSettingsType StringToContentSettingsType(
    const std::string& content_type) {
  for (size_t type = 0; type < arraysize(kContentSettingsTypeNames); ++type) {
    if (content_type == kContentSettingsTypeNames[type])
      return static_cast<ContentSettingsType>(type);
  }
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

const char* ContentSettingsTypeToString(ContentSettingsType type) {
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, arraysize(kContentSettingsTypeNames));
  return kContentSettingsTypeNames[index];
}

bool StringToContentSetting(const std::string& setting_str,
                            ContentSetting* setting) {
  for (size_t type = 0; type < arraysize(kContentSettingNames); ++type) {
    if (setting_str == kContentSettingNames[type]) {
      *setting = static_cast<ContentSetting>(type);
      return true;
    }
  }
  return false;
}

const char* ContentSettingToString(ContentSetting setting) {
  size_t index = static_cast<size_t>(setting);
  DCHECK_LT(index, arraysize(kContentSettingNames));
  return kContentSettingNames[index];
}

}  // namespace extension_content_settings_helpers

