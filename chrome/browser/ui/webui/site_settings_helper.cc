// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/site_settings_helper.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"

namespace site_settings {

const char kSetting[] = "setting";
const char kOrigin[] = "origin";
const char kPolicyProviderId[] = "policy";
const char kSource[] = "source";
const char kEmbeddingOrigin[] = "embeddingOrigin";
const char kPreferencesSource[] = "preference";

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
  {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
  {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
  {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
  {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
  {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
  {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
  {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
  {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, "auto-select-certificate"},
  {CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen"},
  {CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock"},
  {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera"},
  {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
  {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
  {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex"},
  {CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, "push-messaging"},
  {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions"},
#if defined(OS_CHROMEOS)
  {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, "protectedContent"},
#endif
  {CONTENT_SETTINGS_TYPE_KEYGEN, "keygen"},
  {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, "background-sync"},
};

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

// Create a DictionaryValue* that will act as a data source for a single row
// in a HostContentSettingsMap-controlled exceptions table (e.g., cookies).
std::unique_ptr<base::DictionaryValue> GetExceptionForPage(
    const ContentSettingsPattern& pattern,
    const ContentSettingsPattern& secondary_pattern,
    const ContentSetting& setting,
    const std::string& provider_name) {
  base::DictionaryValue* exception = new base::DictionaryValue();
  exception->SetString(kOrigin, pattern.ToString());
  exception->SetString(kEmbeddingOrigin,
                       secondary_pattern == ContentSettingsPattern::Wildcard() ?
                           std::string() :
                           secondary_pattern.ToString());

  std::string setting_string =
      content_settings::ContentSettingToString(setting);
  DCHECK(!setting_string.empty());

  exception->SetString(kSetting, setting_string);
  exception->SetString(kSource, provider_name);
  return base::WrapUnique(exception);
}

void GetExceptionsFromHostContentSettingsMap(const HostContentSettingsMap* map,
                                             ContentSettingsType type,
                                             content::WebUI* web_ui,
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
    if (map->is_off_the_record() && !i->incognito)
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
    this_provider_exceptions.push_back(GetExceptionForPage(
        primary_pattern, secondary_pattern, parent_setting, source));

    // Add the "children" for any embedded settings.
    for (OnePatternSettings::const_iterator j = one_settings.begin();
         j != one_settings.end(); ++j) {
      // Skip the non-embedded setting which we already added above.
      if (j == parent)
        continue;

      ContentSetting content_setting = j->second;
      this_provider_exceptions.push_back(GetExceptionForPage(
          primary_pattern, j->first, content_setting, source));
    }
  }

  // For camera and microphone, we do not have policy exceptions, but we do have
  // the policy-set allowed URLs, which should be displayed in the same manner.
  if (type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    auto& policy_exceptions = all_provider_exceptions
        [HostContentSettingsMap::GetProviderTypeFromSource(kPolicyProviderId)];
    DCHECK(policy_exceptions.empty());
    GetPolicyAllowedUrls(type, &policy_exceptions, web_ui);
  }

  for (auto& one_provider_exceptions : all_provider_exceptions) {
    for (auto& exception : one_provider_exceptions)
      exceptions->Append(std::move(exception));
  }
}

void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    content::WebUI* web_ui) {
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
    exceptions->push_back(GetExceptionForPage(pattern, ContentSettingsPattern(),
                                              CONTENT_SETTING_ALLOW,
                                              kPolicyProviderId));
  }
}

}  // namespace site_settings
