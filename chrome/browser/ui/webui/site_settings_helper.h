// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_ui.h"
#include "extensions/common/extension.h"

class ChooserContextBase;
class HostContentSettingsMap;
class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class ExtensionRegistry;
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
extern const char kDisplayName[];
extern const char kOriginForFavicon[];
extern const char kPolicyProviderId[];
extern const char kSource[];
extern const char kIncognito[];
extern const char kEmbeddingOrigin[];
extern const char kPreferencesSource[];

// Group types.
extern const char kGroupTypeUsb[];

// Returns whether a group name has been registered for the given type.
bool HasRegisteredGroupName(ContentSettingsType type);

// Gets a content settings type from the group name identifier.
ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name);

// Gets a string identifier for the group name.
std::string ContentSettingsTypeToGroupName(ContentSettingsType type);

// Helper function to construct a dictonary for an exception.
std::unique_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito);

// Helper function to construct a dictonary for a hosted app exception.
void AddExceptionForHostedApp(const std::string& url_pattern,
    const extensions::Extension& app, base::ListValue* exceptions);

// Fills in |exceptions| with Values for the given |type| from |map|.
// If |filter| is not null then only exceptions with matching primary patterns
// will be returned.
void GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito,
    const std::string* filter,
    base::ListValue* exceptions);

// Fills in object saying what the current settings is for the category (such as
// enabled or blocked) and the source of that setting (such preference, policy,
// or extension).
void GetContentCategorySetting(
    const HostContentSettingsMap* map,
    ContentSettingsType content_type,
    base::DictionaryValue* object);

// Returns exceptions constructed from the policy-set allowed URLs
// for the content settings |type| mic or camera.
void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito);

// This struct facilitates lookup of a chooser context factory function by name
// for a given content settings type and is declared early so that it can used
// by functions below.
struct ChooserTypeNameEntry {
  ContentSettingsType type;
  ChooserContextBase* (*get_context)(Profile*);
  const char* name;
  const char* ui_name_key;
};

ChooserContextBase* GetUsbChooserContext(Profile* profile);

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

const ChooserTypeNameEntry kChooserTypeGroupNames[] = {
    {CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, &GetUsbChooserContext,
     kGroupTypeUsb, "name"},
};

const ChooserTypeNameEntry* ChooserTypeFromGroupName(const std::string& name);

// Fills in |exceptions| with Values for the given |chooser_type| from map.
void GetChooserExceptionsFromProfile(
    Profile* profile,
    bool incognito,
    const ChooserTypeNameEntry& chooser_type,
    base::ListValue* exceptions);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
