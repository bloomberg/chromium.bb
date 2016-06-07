// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_ui.h"

class HostContentSettingsMap;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace site_settings {

// Maps from a secondary pattern to a setting.
typedef std::map<ContentSettingsPattern, ContentSetting>
    OnePatternSettings;
// Maps from a primary pattern/source pair to a OnePatternSettings. All the
// mappings in OnePatternSettings share the given primary pattern and source.
typedef std::map<std::pair<ContentSettingsPattern, std::string>,
                 OnePatternSettings>
    AllPatternsSettings;

extern const char kSetting[];
extern const char kOrigin[];
extern const char kPolicyProviderId[];
extern const char kSource[];
extern const char kEmbeddingOrigin[];
extern const char kPreferencesSource[];

// Returns whether a group name has been registered for the given type.
bool HasRegisteredGroupName(ContentSettingsType type);

// Gets a content settings type from the group name identifier.
ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name);

// Gets a string identifier for the group name.
std::string ContentSettingsTypeToGroupName(ContentSettingsType type);

// Fills in |exceptions| with Values for the given |type| from |map|.
void GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    content::WebUI* web_ui,
    base::ListValue* exceptions);

// Returns exceptions constructed from the policy-set allowed URLs
// for the content settings |type| mic or camera.
void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    content::WebUI* web_ui);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
