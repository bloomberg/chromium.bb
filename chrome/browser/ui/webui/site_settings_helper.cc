// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/site_settings_helper.h"

#include <functional>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/permissions/chooser_context_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"

namespace site_settings {

const char kAppName[] = "appName";
const char kAppId[] = "appId";
const char kSetting[] = "setting";
const char kOrigin[] = "origin";
const char kDisplayName[] = "displayName";
const char kOriginForFavicon[] = "originForFavicon";
const char kPolicyProviderId[] = "policy";
const char kSource[] = "source";
const char kIncognito[] = "incognito";
const char kEmbeddingOrigin[] = "embeddingOrigin";
const char kPreferencesSource[] = "preference";
const char kObject[] = "object";
const char kObjectName[] = "objectName";

const char kGroupTypeUsb[] = "usb-devices";

ChooserContextBase* GetUsbChooserContext(Profile* profile) {
  return reinterpret_cast<ChooserContextBase*>(
      UsbChooserContextFactory::GetForProfile(profile));
}

namespace {

// Maps from the UI string to the object it represents (for sorting purposes).
typedef std::multimap<std::string, const base::DictionaryValue*> SortedObjects;

// Maps from a secondary URL to the set of objects it has permission to access.
typedef std::map<GURL, SortedObjects> OneOriginObjects;

// Maps from a primary URL/source pair to a OneOriginObjects. All the mappings
// in OneOriginObjects share the given primary URL and source.
typedef std::map<std::pair<GURL, std::string>, OneOriginObjects>
    AllOriginObjects;

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
    {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
    {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
    {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
    {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
    {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, "auto-select-certificate"},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic"},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera"},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex"},
    {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions"},
#if defined(OS_CHROMEOS)
    {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, "protectedContent"},
#endif
    {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, "background-sync"},
};

}  // namespace

bool HasRegisteredGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type)
      return true;
  }
  return false;
}

ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (name == kContentSettingsTypeGroupNames[i].name)
      return kContentSettingsTypeGroupNames[i].type;
  }

  NOTREACHED() << name << " is not a recognized content settings type.";
  return CONTENT_SETTINGS_TYPE_DEFAULT;
}

std::string ContentSettingsTypeToGroupName(ContentSettingsType type) {
  for (size_t i = 0; i < arraysize(kContentSettingsTypeGroupNames); ++i) {
    if (type == kContentSettingsTypeGroupNames[i].type)
      return kContentSettingsTypeGroupNames[i].name;
  }

  NOTREACHED() << type << " is not a recognized content settings type.";
  return std::string();
}

// Add an "Allow"-entry to the list of |exceptions| for a |url_pattern| from
// the web extent of a hosted |app|.
void AddExceptionForHostedApp(const std::string& url_pattern,
    const extensions::Extension& app, base::ListValue* exceptions) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_ALLOW);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, url_pattern);
  exception->SetString(site_settings::kDisplayName, url_pattern);
  exception->SetString(site_settings::kEmbeddingOrigin, url_pattern);
  exception->SetString(site_settings::kSource, "HostedApp");
  exception->SetBoolean(site_settings::kIncognito, false);
  exception->SetString(kAppName, app.name());
  exception->SetString(kAppId, app.id());
  exceptions->Append(std::move(exception));
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
std::unique_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const std::string& display_name,
    const ContentSetting& setting,
    const std::string& provider_name,
    bool incognito) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kDisplayName, display_name);
  exception->SetString(kEmbeddingOrigin,
                       secondary_pattern == ContentSettingsPattern::Wildcard() ?
                           std::string() :
                           secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kSource, provider_name);
  exception->SetBoolean(kIncognito, incognito);
  return base::WrapUnique(exception);
}

std::string GetDisplayName(
    const ContentSettingsPattern& pattern,
    const extensions::ExtensionRegistry* extension_registry) {
  if (extension_registry &&
      pattern.GetScheme() == ContentSettingsPattern::SCHEME_CHROMEEXTENSION) {
    GURL url(pattern.ToString());
    // For the extension scheme, the pattern must be a valid URL.
    DCHECK(url.is_valid());
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            url.host(), extensions::ExtensionRegistry::EVERYTHING);
    if (extension)
      return extension->name();
  }
  return pattern.ToString();
}

void GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito,
    const std::string* filter,
    base::ListValue* exceptions) {
  ContentSettingsForOneType entries;
  map->GetSettingsForOneType(type, std::string(), &entries);
  // Group settings by primary_pattern.
  AllPatternsSettings all_patterns_settings;
  for (ContentSettingsForOneType::iterator i = entries.begin();
       i != entries.end(); ++i) {
    // Don't add default settings.
    if (i->primary_pattern == ContentSettingsPattern::Wildcard() &&
        i->secondary_pattern == ContentSettingsPattern::Wildcard() &&
        i->source != kPreferencesSource) {
      continue;
    }

    // Off-the-record HostContentSettingsMap contains incognito content settings
    // as well as normal content settings. Here, we use the incongnito settings
    // only.
    if (map->is_incognito() && !i->incognito)
      continue;

    if (filter && i->primary_pattern.ToString() != *filter)
      continue;

    all_patterns_settings[std::make_pair(i->primary_pattern, i->source)]
        [i->secondary_pattern] = i->setting;
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  // |all_patterns_settings| is sorted from the lowest precedence pattern to
  // the highest (see operator< in ContentSettingsPattern), so traverse it in
  // reverse to show the patterns with the highest precedence (the more specific
  // ones) on the top.
  for (AllPatternsSettings::reverse_iterator i = all_patterns_settings.rbegin();
       i != all_patterns_settings.rend();
       ++i) {
    const ContentSettingsPattern& primary_pattern = i->first.first;
    const OnePatternSettings& one_settings = i->second;
    const std::string display_name =
        GetDisplayName(primary_pattern, extension_registry);

    // The "parent" entry either has an identical primary and secondary pattern,
    // or has a wildcard secondary. The two cases are indistinguishable in the
    // UI.
    OnePatternSettings::const_iterator parent =
        one_settings.find(primary_pattern);
    if (parent == one_settings.end())
      parent = one_settings.find(ContentSettingsPattern::Wildcard());

    const std::string& source = i->first.second;
    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add the "parent" entry for the non-embedded setting.
    ContentSetting parent_setting =
        parent == one_settings.end() ? CONTENT_SETTING_DEFAULT : parent->second;
    const ContentSettingsPattern& secondary_pattern =
        parent == one_settings.end() ? primary_pattern : parent->first;
    this_provider_exceptions.push_back(
        GetExceptionForPage(primary_pattern, secondary_pattern, display_name,
                            parent_setting, source, incognito));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(
          GetExceptionForPage(primary_pattern, j->first, display_name,
                              content_setting, source, incognito));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    auto& policy_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(kPolicyProviderId)];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions, extension_registry, web_ui,
                         incognito);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

void GetContentCategorySetting(
    const HostContentSettingsMap* map,
    ContentSettingsType content_type,
    base::DictionaryValue* object) {
  std::string provider;
  std::string setting = content_settings::ContentSettingToString(
      map->GetDefaultContentSetting(content_type, &provider));
  DCHECK(!setting.empty());

  object->SetString(site_settings::kSetting, setting);
  if (provider != "default")
    object->SetString(site_settings::kSource, provider);
}

void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    const extensions::ExtensionRegistry* extension_registry,
    content::WebUI* web_ui,
    bool incognito) {
  DCHECK(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);

  PrefService* prefs = Profile::FromWebUI(web_ui)->GetPrefs();
  const base::ListValue* policy_urls = prefs->GetList(
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC
          ? prefs::kAudioCaptureAllowedUrls
          : prefs::kVideoCaptureAllowedUrls);

  // Convert the URLs to |ContentSettingsPattern|s. Ignore any invalid ones.
  std::vector<ContentSettingsPattern> patterns;
  for (const auto& entry : *policy_urls) {
    std::string url;
    bool valid_string = entry->GetAsString(&url);
    if (!valid_string)
      continue;

    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(url);
    if (!pattern.IsValid())
      continue;

    patterns.push_back(pattern);
  }

  // The patterns are shown in the UI in a reverse order defined by
  // |ContentSettingsPattern::operator<|.
  std::sort(
      patterns.begin(), patterns.end(), std::greater<ContentSettingsPattern>());

  for (const ContentSettingsPattern& pattern : patterns) {
    std::string display_name = GetDisplayName(pattern, extension_registry);
    exceptions->push_back(GetExceptionForPage(
        pattern, ContentSettingsPattern(), display_name, CONTENT_SETTING_ALLOW,
        kPolicyProviderId, incognito));
  }
}

const ChooserTypeNameEntry* ChooserTypeFromGroupName(const std::string& name) {
  for (const auto& chooser_type : kChooserTypeGroupNames) {
    if (chooser_type.name == name)
      return &chooser_type;
  }
  return nullptr;
}

// Create a DictionaryValue* that will act as a data source for a single row
// in a chooser permission exceptions table.
std::unique_ptr<base::DictionaryValue> GetChooserExceptionForPage(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const std::string& provider_name,
    bool incognito,
    const std::string& name,
    const base::DictionaryValue* object) {
  std::unique_ptr<base::DictionaryValue> exception(new base::DictionaryValue());

  std::string setting_string =
      content_settings::ContentSettingToString(CONTENT_SETTING_DEFAULT);
  DCHECK(!setting_string.empty());

  exception->SetString(site_settings::kSetting, setting_string);
  exception->SetString(site_settings::kOrigin, requesting_origin.spec());
  exception->SetString(site_settings::kDisplayName, requesting_origin.spec());
  exception->SetString(
      site_settings::kEmbeddingOrigin, embedding_origin.spec());
  exception->SetString(site_settings::kSource, provider_name);
  exception->SetBoolean(kIncognito, incognito);
  if (object) {
    exception->SetString(kObjectName, name);
    exception->Set(kObject, object->CreateDeepCopy());
  }
  return exception;
}

void GetChooserExceptionsFromProfile(
    Profile* profile,
    bool incognito,
    const ChooserTypeNameEntry& chooser_type,
    base::ListValue* exceptions) {
  if (incognito) {
    if (!profile->HasOffTheRecordProfile())
      return;
    profile = profile->GetOffTheRecordProfile();
  }

  ChooserContextBase* chooser_context = chooser_type.get_context(profile);
  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      chooser_context->GetAllGrantedObjects();
  AllOriginObjects all_origin_objects;
  for (const auto& object : objects) {
    std::string name;
    bool found = object->object.GetString(chooser_type.ui_name_key, &name);
    DCHECK(found);
    // It is safe for this structure to hold references into |objects| because
    // they are both destroyed at the end of this function.
    all_origin_objects[make_pair(object->requesting_origin,
                                 object->source)][object->embedding_origin]
        .insert(make_pair(name, &object->object));
  }

  // Keep the exceptions sorted by provider so they will be displayed in
  // precedence order.
  std::vector<std::unique_ptr<base::DictionaryValue>>
      all_provider_exceptions[HostContentSettingsMap::NUM_PROVIDER_TYPES];

  for (const auto& all_origin_objects_entry : all_origin_objects) {
    const GURL& requesting_origin = all_origin_objects_entry.first.first;
    const std::string& source = all_origin_objects_entry.first.second;
    const OneOriginObjects& one_origin_objects =
        all_origin_objects_entry.second;

    auto& this_provider_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(source)];

    // Add entries for any non-embedded origins.
    bool has_embedded_entries = false;
    for (const auto& one_origin_objects_entry : one_origin_objects) {
      const GURL& embedding_origin = one_origin_objects_entry.first;
      const SortedObjects& sorted_objects = one_origin_objects_entry.second;

      // Skip the embedded settings which will be added below.
      if (requesting_origin != embedding_origin) {
        has_embedded_entries = true;
        continue;
      }

      for (const auto& sorted_objects_entry : sorted_objects) {
        this_provider_exceptions.push_back(GetChooserExceptionForPage(
            requesting_origin, embedding_origin, source, incognito,
            sorted_objects_entry.first,
            sorted_objects_entry.second));
      }
    }

    if (has_embedded_entries) {
      // Add a "parent" entry that simply acts as a heading for all entries
      // where |requesting_origin| has been embedded.
      this_provider_exceptions.push_back(
          GetChooserExceptionForPage(requesting_origin, requesting_origin,
                                     source, incognito, std::string(),
                                     nullptr));

      // Add the "children" for any embedded settings.
      for (const auto& one_origin_objects_entry : one_origin_objects) {
        const GURL& embedding_origin = one_origin_objects_entry.first;
        const SortedObjects& sorted_objects = one_origin_objects_entry.second;

        // Skip the non-embedded setting which we already added above.
        if (requesting_origin == embedding_origin)
          continue;

        for (const auto& sorted_objects_entry : sorted_objects) {
          this_provider_exceptions.push_back(GetChooserExceptionForPage(
              requesting_origin, embedding_origin, source, incognito,
              sorted_objects_entry.first, sorted_objects_entry.second));
        }
      }
    }
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

}  // namespace site_settings
