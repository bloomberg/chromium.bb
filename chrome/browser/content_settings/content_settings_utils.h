// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_

#include <string>
#include <utility>

#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace base {
class Value;
}

class GURL;
class HostContentSettingsMap;

namespace content_settings {

class ProviderInterface;
class RuleIterator;

typedef std::pair<ContentSettingsPattern, ContentSettingsPattern> PatternPair;

std::string GetTypeName(ContentSettingsType type);

bool GetTypeFromName(const std::string& name,
                     ContentSettingsType* return_setting);

// Converts |Value| to |ContentSetting|.
ContentSetting ValueToContentSetting(const base::Value* value);

// Converts a |Value| to a |ContentSetting|. Returns true if |value| encodes
// a valid content setting, false otherwise. Note that |CONTENT_SETTING_DEFAULT|
// is encoded as a NULL value, so it is not allowed as an integer value.
bool ParseContentSettingValue(const base::Value* value,
                              ContentSetting* setting);

PatternPair ParsePatternString(const std::string& pattern_str);

std::string CreatePatternString(
    const ContentSettingsPattern& item_pattern,
    const ContentSettingsPattern& top_level_frame_pattern);

// Caller takes the ownership of the returned |base::Value*|.
base::Value* GetContentSettingValueAndPatterns(
    RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern);

base::Value* GetContentSettingValueAndPatterns(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern);

base::Value* GetContentSettingValue(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito);

ContentSetting GetContentSetting(
    const ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool include_incognito);

// Populates |rules| with content setting rules for content types that are
// handled by the renderer.
void GetRendererContentSettingRules(const HostContentSettingsMap* map,
                                    RendererContentSettingRules* rules);

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_UTILS_H_
