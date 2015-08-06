// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_default_provider.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_registry.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/plugins_field_trial.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

// Obsolete prefs to be removed from the pref file.
// TODO(msramek): Remove this cleanup code after two releases (i.e. in M48).
const char kObsoleteDefaultContentSettings[] =
    "profile.default_content_settings";
const char kObsoleteMigratedDefaultContentSettings[] =
    "profile.migrated_default_content_settings";

ContentSetting GetDefaultValue(ContentSettingsType type) {
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS)
    return PluginsFieldTrial::GetDefaultPluginsContentSetting();

  const WebsiteSettingsInfo* info =
      WebsiteSettingsRegistry::GetInstance()->Get(type);
  const base::Value* initial_default = info->initial_default_value();
  if (!initial_default)
    return CONTENT_SETTING_DEFAULT;
  int result = 0;
  bool success = initial_default->GetAsInteger(&result);
  DCHECK(success);
  return static_cast<ContentSetting>(result);
}

const std::string& GetPrefName(ContentSettingsType type) {
  return WebsiteSettingsRegistry::GetInstance()
      ->Get(type)
      ->default_value_pref_name();
}

class DefaultRuleIterator : public RuleIterator {
 public:
  explicit DefaultRuleIterator(const base::Value* value) {
    if (value)
      value_.reset(value->DeepCopy());
  }

  bool HasNext() const override { return value_.get() != NULL; }

  Rule Next() override {
    DCHECK(value_.get());
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(),
                value_.release());
  }

 private:
  scoped_ptr<base::Value> value_;
};

}  // namespace

// static
void DefaultProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Register the default settings' preferences.
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    registry->RegisterIntegerPref(GetPrefName(type), GetDefaultValue(type),
                                  PrefRegistrationFlagsForType(type));
  }

  // Whether the deprecated mediastream default setting has already been
  // migrated into microphone and camera default settings.
  registry->RegisterBooleanPref(prefs::kMigratedDefaultMediaStreamSetting,
                                false);

  // Obsolete prefs -------------------------------------------------------

  // The deprecated dictionary preference.
  registry->RegisterDictionaryPref(
      kObsoleteDefaultContentSettings,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Whether the deprecated dictionary preference has already been migrated
  // into the individual preferences in this profile.
  registry->RegisterBooleanPref(
      kObsoleteMigratedDefaultContentSettings,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

DefaultProvider::DefaultProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      is_incognito_(incognito),
      updating_preferences_(false) {
  DCHECK(prefs_);

  // Remove the obsolete preferences from the pref file.
  DiscardObsoletePreferences();

  // Read global defaults.
  ReadDefaultSettings();

  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultCookiesSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_COOKIES))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultImagesSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_IMAGES))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultJavaScriptSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_JAVASCRIPT))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPluginsSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_PLUGINS))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPopupsSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_POPUPS))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultLocationSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_GEOLOCATION))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultNotificationsSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_NOTIFICATIONS))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMouseCursorSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_MOUSELOCK))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMediaStreamSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_MEDIASTREAM))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMIDISysExSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_MIDI_SYSEX))),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPushMessagingSetting",
      IntToContentSetting(prefs_->GetInteger(
          GetPrefName(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING))),
      CONTENT_SETTING_NUM_SETTINGS);

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::Bind(
      &DefaultProvider::OnPreferenceChanged, base::Unretained(this));
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    pref_change_registrar_.Add(GetPrefName(type), callback);
  }
}

DefaultProvider::~DefaultProvider() {
}

bool DefaultProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    base::Value* in_value) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  // Ignore non default settings
  if (primary_pattern != ContentSettingsPattern::Wildcard() ||
      secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return false;
  }

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_incognito_)
    return false;

  // Put |in_value| in a scoped pointer to ensure that it gets cleaned up
  // properly if we don't pass on the ownership.
  scoped_ptr<base::Value> value(in_value);
  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    // Lock the memory map access, so that values are not read by
    // |GetRuleIterator| at the same time as they are written here. Do not lock
    // the preference access though; preference updates send out notifications
    // whose callbacks may try to reacquire the lock on the same thread.
    {
      base::AutoLock lock(lock_);
      ChangeSetting(content_type, value.get());
    }
    WriteToPref(content_type, value.get());
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  ResourceIdentifier());

  return true;
}

RuleIterator* DefaultProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  base::AutoLock lock(lock_);
  if (resource_identifier.empty()) {
    ValueMap::const_iterator it(default_settings_.find(content_type));
    if (it != default_settings_.end()) {
      return new DefaultRuleIterator(it->second.get());
    }
    NOTREACHED();
  }
  return new EmptyRuleIterator();
}

void DefaultProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // TODO(markusheintz): This method is only called when the
  // |DesktopNotificationService| calls |ClearAllSettingsForType| method on the
  // |HostContentSettingsMap|. Don't implement this method yet, otherwise the
  // default notification settings will be cleared as well.
}

void DefaultProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = NULL;
}

void DefaultProvider::ReadDefaultSettings() {
  base::AutoLock lock(lock_);
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    ChangeSetting(type, ReadFromPref(type).get());
  }
}

bool DefaultProvider::IsValueEmptyOrDefault(ContentSettingsType content_type,
                                            base::Value* value) {
  if (!value) return true;
  return ValueToContentSetting(value) == GetDefaultValue(content_type);
}

void DefaultProvider::ChangeSetting(ContentSettingsType content_type,
                                    base::Value* value) {
  if (!value) {
    default_settings_[content_type].reset(
        ContentSettingToValue(GetDefaultValue(content_type)).release());
  } else {
    default_settings_[content_type].reset(value->DeepCopy());
  }
}

void DefaultProvider::WriteToPref(ContentSettingsType content_type,
                                  base::Value* value) {
  if (IsValueEmptyOrDefault(content_type, value)) {
    prefs_->ClearPref(GetPrefName(content_type));
    return;
  }

  int int_value = GetDefaultValue(content_type);
  bool is_integer = value->GetAsInteger(&int_value);
  DCHECK(is_integer);
  prefs_->SetInteger(GetPrefName(content_type), int_value);
}

void DefaultProvider::OnPreferenceChanged(const std::string& name) {
  DCHECK(CalledOnValidThread());
  if (updating_preferences_)
    return;

  // Find out which content setting the preference corresponds to.
  ContentSettingsType content_type = CONTENT_SETTINGS_TYPE_DEFAULT;

  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    if (GetPrefName(type) == name) {
      content_type = type;
      break;
    }
  }

  if (content_type == CONTENT_SETTINGS_TYPE_DEFAULT) {
    NOTREACHED() << "A change of the preference " << name << " was observed, "
                    "but the preference could not be mapped to a content "
                    "settings type.";
    return;
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    // Lock the memory map access, so that values are not read by
    // |GetRuleIterator| at the same time as they are written here. Do not lock
    // the preference access though; preference updates send out notifications
    // whose callbacks may try to reacquire the lock on the same thread.
    {
      base::AutoLock lock(lock_);
      ChangeSetting(content_type, ReadFromPref(content_type).get());
    }
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  ResourceIdentifier());
}

scoped_ptr<base::Value> DefaultProvider::ReadFromPref(
    ContentSettingsType content_type) {
  int int_value = prefs_->GetInteger(GetPrefName(content_type));
  return ContentSettingToValue(IntToContentSetting(int_value)).Pass();
}

void DefaultProvider::ForceDefaultsToBeExplicit(ValueMap* value_map) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = static_cast<ContentSettingsType>(i);
    if (!(*value_map)[type].get()) {
      (*value_map)[type].reset(ContentSettingToValue(
          GetDefaultValue(type)).release());
    }
  }
}

void DefaultProvider::DiscardObsoletePreferences() {
  prefs_->ClearPref(kObsoleteDefaultContentSettings);
  prefs_->ClearPref(kObsoleteMigratedDefaultContentSettings);
}

}  // namespace content_settings
