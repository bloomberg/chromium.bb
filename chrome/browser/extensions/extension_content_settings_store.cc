// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_content_settings_store.h"

#include <set>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_content_settings_api_constants.h"
#include "chrome/browser/extensions/extension_content_settings_helpers.h"
#include "content/browser/browser_thread.h"

namespace helpers = extension_content_settings_helpers;
namespace keys = extension_content_settings_api_constants;

namespace {

bool ComparePatternPairs(const ContentSettingsPattern& first_primary,
                         const ContentSettingsPattern& first_secondary,
                         const ContentSettingsPattern& second_primary,
                         const ContentSettingsPattern& second_secondary) {
  ContentSettingsPattern::Relation relation =
      first_primary.Compare(second_primary);
  if (relation == ContentSettingsPattern::SUCCESSOR)
    return true;
  if (relation == ContentSettingsPattern::PREDECESSOR)
    return false;
  return (first_secondary.Compare(second_secondary) ==
      ContentSettingsPattern::SUCCESSOR);
}

}  // namespace

using content_settings::ResourceIdentifier;

struct ExtensionContentSettingsStore::ExtensionEntry {
  // Installation time of the extension.
  base::Time install_time;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Content settings.
  ContentSettingSpecList settings;
  // Persistent incognito content settings.
  ContentSettingSpecList incognito_persistent_settings;
  // Session-only incognito content settings.
  ContentSettingSpecList incognito_session_only_settings;
};

ExtensionContentSettingsStore::ExtensionContentSettingsStore() {
  DCHECK(OnCorrectThread());
}

ExtensionContentSettingsStore::~ExtensionContentSettingsStore() {
  STLDeleteValues(&entries_);
}

void ExtensionContentSettingsStore::SetExtensionContentSetting(
    const std::string& ext_id,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    ContentSetting setting,
    ExtensionPrefsScope scope) {
  {
    base::AutoLock lock(lock_);
    ContentSettingSpecList* setting_spec_list =
        GetContentSettingSpecList(ext_id, scope);

    // Find |ContentSettingSpec|.
    ContentSettingSpecList::iterator setting_spec = setting_spec_list->begin();
    while (setting_spec != setting_spec_list->end()) {
      if (setting_spec->primary_pattern == primary_pattern &&
          setting_spec->secondary_pattern == secondary_pattern &&
          setting_spec->content_type == type &&
          setting_spec->resource_identifier == identifier) {
        break;
      }
      ++setting_spec;
    }
    if (setting_spec == setting_spec_list->end()) {
      setting_spec_list->push_back(ContentSettingSpec(primary_pattern,
                                                      secondary_pattern,
                                                      type,
                                                      identifier,
                                                      setting));
    } else if (setting != CONTENT_SETTING_DEFAULT) {
      // Update setting.
      setting_spec->setting = setting;
    } else {
      setting_spec_list->erase(setting_spec);
    }
  }

  // Send notification that content settings changed.
  // TODO(markusheintz): Notifications should only be sent if the set content
  // setting is effective and not hidden by another setting of another
  // extension installed more recently.
  NotifyOfContentSettingChanged(ext_id,
                                scope != kExtensionPrefsScopeRegular);
}

void ExtensionContentSettingsStore::RegisterExtension(
    const std::string& ext_id,
    const base::Time& install_time,
    bool is_enabled) {
  base::AutoLock lock(lock_);
  ExtensionEntryMap::iterator i = entries_.find(ext_id);
  if (i != entries_.end())
    delete i->second;

  entries_[ext_id] = new ExtensionEntry;
  entries_[ext_id]->install_time = install_time;
  entries_[ext_id]->enabled = is_enabled;
}

void ExtensionContentSettingsStore::UnregisterExtension(
    const std::string& ext_id) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::iterator i = entries_.find(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    delete i->second;
    entries_.erase(i);
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

void ExtensionContentSettingsStore::SetExtensionState(
    const std::string& ext_id, bool is_enabled) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
    if (i == entries_.end())
      return;
    notify = !i->second->settings.empty();
    notify_incognito = !i->second->incognito_persistent_settings.empty() ||
                       !i->second->incognito_session_only_settings.empty();

    i->second->enabled = is_enabled;
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

ExtensionContentSettingsStore::ContentSettingSpecList*
    ExtensionContentSettingsStore::GetContentSettingSpecList(
        const std::string& ext_id,
        ExtensionPrefsScope scope) {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  if (i != entries_.end()) {
    switch (scope) {
      case kExtensionPrefsScopeRegular:
        return &(i->second->settings);
      case kExtensionPrefsScopeIncognitoPersistent:
        return &(i->second->incognito_persistent_settings);
      case kExtensionPrefsScopeIncognitoSessionOnly:
        return &(i->second->incognito_session_only_settings);
    }
  }
  return NULL;
}

const ExtensionContentSettingsStore::ContentSettingSpecList*
    ExtensionContentSettingsStore::GetContentSettingSpecList(
        const std::string& ext_id,
        ExtensionPrefsScope scope) const {
  ExtensionEntryMap::const_iterator i = entries_.find(ext_id);
  if (i != entries_.end()) {
    switch (scope) {
      case kExtensionPrefsScopeRegular:
        return &(i->second->settings);
      case kExtensionPrefsScopeIncognitoPersistent:
        return &(i->second->incognito_persistent_settings);
      case kExtensionPrefsScopeIncognitoSessionOnly:
        return &(i->second->incognito_session_only_settings);
    }
  }
  return NULL;
}

ContentSetting ExtensionContentSettingsStore::GetContentSettingFromSpecList(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    const ContentSettingSpecList& setting_spec_list) const {
  ContentSettingSpecList::const_iterator winner_spec = setting_spec_list.end();

  for (ContentSettingSpecList::const_iterator spec = setting_spec_list.begin();
       spec != setting_spec_list.end(); ++spec) {
    if (!spec->primary_pattern.Matches(primary_url) ||
        !spec->secondary_pattern.Matches(secondary_url) ||
        spec->content_type != type ||
        spec->resource_identifier != identifier) {
      continue;
    }

    if (winner_spec == setting_spec_list.end() ||
        ComparePatternPairs(winner_spec->primary_pattern,
                            winner_spec->secondary_pattern,
                            spec->primary_pattern,
                            spec->secondary_pattern)) {
      winner_spec = spec;
    }
  }

  return (winner_spec != setting_spec_list.end()) ? winner_spec->setting
                                                  : CONTENT_SETTING_DEFAULT;
}

ContentSetting ExtensionContentSettingsStore::GetEffectiveContentSetting(
    const GURL& embedded_url,
    const GURL& top_level_url,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    bool incognito) const {
  base::AutoLock lock(lock_);

  base::Time winners_install_time;
  ContentSetting winner_setting = CONTENT_SETTING_DEFAULT;

  ExtensionEntryMap::const_iterator i;
  for (i = entries_.begin(); i != entries_.end(); ++i) {
    const base::Time& install_time = i->second->install_time;
    const bool enabled = i->second->enabled;

    if (!enabled)
      continue;
    if (install_time < winners_install_time)
      continue;

    ContentSetting setting = CONTENT_SETTING_DEFAULT;
    if (incognito) {
      // Try session-only incognito setting first.
      setting = GetContentSettingFromSpecList(
          embedded_url, top_level_url, type, identifier,
          i->second->incognito_session_only_settings);
      if (setting == CONTENT_SETTING_DEFAULT) {
        // Next, persistent incognito setting.
        setting = GetContentSettingFromSpecList(
            embedded_url, top_level_url, type, identifier,
            i->second->incognito_persistent_settings);
      }
    }
    if (setting == CONTENT_SETTING_DEFAULT) {
      // Then, non-incognito setting.
      setting = GetContentSettingFromSpecList(embedded_url, top_level_url, type,
                                              identifier, i->second->settings);
    }

    if (setting != CONTENT_SETTING_DEFAULT) {
      winners_install_time = install_time;
      winner_setting = setting;
    }
  }

  return winner_setting;
}

void ExtensionContentSettingsStore::ClearContentSettingsForExtension(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    ContentSettingSpecList* setting_spec_list =
        GetContentSettingSpecList(ext_id, scope);
    notify = !setting_spec_list->empty();
    setting_spec_list->clear();
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

// static
void ExtensionContentSettingsStore::AddRules(
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    const ContentSettingSpecList* setting_spec_list,
    content_settings::ProviderInterface::Rules* rules) {
  ContentSettingSpecList::const_iterator it;
  for (it = setting_spec_list->begin(); it != setting_spec_list->end(); ++it) {
    if (it->content_type == type && it->resource_identifier == identifier) {
      rules->push_back(content_settings::ProviderInterface::Rule(
          it->primary_pattern, it->secondary_pattern, it->setting));
    }
  }
}

void ExtensionContentSettingsStore::GetContentSettingsForContentType(
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    bool incognito,
    content_settings::ProviderInterface::Rules* rules) const {
  base::AutoLock lock(lock_);
  ExtensionEntryMap::const_iterator ext_it;
  for (ext_it = entries_.begin(); ext_it != entries_.end(); ++ext_it) {
    if (incognito) {
      AddRules(type, identifier,
               GetContentSettingSpecList(
                   ext_it->first,
                   kExtensionPrefsScopeIncognitoPersistent),
               rules);
      AddRules(type, identifier,
               GetContentSettingSpecList(
                   ext_it->first,
                   kExtensionPrefsScopeIncognitoSessionOnly),
                rules);
    } else {
      AddRules(type, identifier,
               GetContentSettingSpecList(
                   ext_it->first,
                   kExtensionPrefsScopeRegular),
               rules);
    }
  }
}

ListValue* ExtensionContentSettingsStore::GetSettingsForExtension(
    const std::string& extension_id,
    ExtensionPrefsScope scope) const {
  base::AutoLock lock(lock_);
  const ContentSettingSpecList* setting_spec_list =
      GetContentSettingSpecList(extension_id, scope);
  if (!setting_spec_list)
    return NULL;
  ListValue* settings = new ListValue();
  ContentSettingSpecList::const_iterator it;
  for (it = setting_spec_list->begin(); it != setting_spec_list->end(); ++it) {
    DictionaryValue* setting_dict = new DictionaryValue();
    setting_dict->SetString(keys::kPrimaryPatternKey,
                            it->primary_pattern.ToString());
    setting_dict->SetString(keys::kSecondaryPatternKey,
                            it->secondary_pattern.ToString());
    setting_dict->SetString(
        keys::kContentSettingsTypeKey,
        helpers::ContentSettingsTypeToString(it->content_type));
    setting_dict->SetString(keys::kResourceIdentifierKey,
                            it->resource_identifier);
    setting_dict->SetString(keys::kContentSettingKey,
                            helpers::ContentSettingToString(it->setting));
    settings->Append(setting_dict);
  }
  return settings;
}

void ExtensionContentSettingsStore::SetExtensionContentSettingsFromList(
    const std::string& extension_id,
    const ListValue* list,
    ExtensionPrefsScope scope) {
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    if ((*it)->GetType() != Value::TYPE_DICTIONARY) {
      NOTREACHED();
      continue;
    }
    DictionaryValue* dict = static_cast<DictionaryValue*>(*it);
    std::string primary_pattern_str;
    dict->GetString(keys::kPrimaryPatternKey, &primary_pattern_str);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(primary_pattern_str);
    DCHECK(primary_pattern.IsValid());

    std::string secondary_pattern_str;
    dict->GetString(keys::kSecondaryPatternKey, &secondary_pattern_str);
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromString(secondary_pattern_str);
    DCHECK(secondary_pattern.IsValid());

    std::string content_settings_type_str;
    dict->GetString(keys::kContentSettingsTypeKey, &content_settings_type_str);
    ContentSettingsType content_settings_type =
        helpers::StringToContentSettingsType(content_settings_type_str);
    DCHECK_NE(CONTENT_SETTINGS_TYPE_DEFAULT, content_settings_type);

    std::string resource_identifier;
    dict->GetString(keys::kResourceIdentifierKey, &resource_identifier);

    std::string content_setting_string;
    dict->GetString(keys::kContentSettingKey, &content_setting_string);
    ContentSetting setting = CONTENT_SETTING_DEFAULT;
    bool result =
        helpers::StringToContentSetting(content_setting_string, &setting);
    DCHECK(result);

    SetExtensionContentSetting(extension_id,
                               primary_pattern,
                               secondary_pattern,
                               content_settings_type,
                               resource_identifier,
                               setting,
                               scope);
  }
}

void ExtensionContentSettingsStore::AddObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.AddObserver(observer);
}

void ExtensionContentSettingsStore::RemoveObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.RemoveObserver(observer);
}

ExtensionContentSettingsStore::ContentSettingSpec::ContentSettingSpec(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    ContentSetting setting)
    : primary_pattern(primary_pattern),
      secondary_pattern(secondary_pattern),
      content_type(type),
      resource_identifier(identifier),
      setting(setting) {
}

void ExtensionContentSettingsStore::NotifyOfContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  FOR_EACH_OBSERVER(
      ExtensionContentSettingsStore::Observer,
      observers_,
      OnContentSettingChanged(extension_id, incognito));
}

bool ExtensionContentSettingsStore::OnCorrectThread() {
  // If there is no UI thread, we're most likely in a unit test.
  return !BrowserThread::IsWellKnownThread(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI);
}
