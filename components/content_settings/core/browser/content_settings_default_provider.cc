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
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "url/gurl.h"

namespace {

struct DefaultContentSettingInfo {
  // The profile preference associated with this default setting.
  const char* pref_name;

  // The default value of this default setting.
  const ContentSetting default_value;

  // Whether this preference should be synced.
  const bool syncable;
};

// The corresponding preference, default value and syncability for each
// default content setting. This array must be kept in sync with the enum
// |ContentSettingsType|.
const DefaultContentSettingInfo kDefaultSettings[] = {
    {prefs::kDefaultCookiesSetting, CONTENT_SETTING_ALLOW, true},
    {prefs::kDefaultImagesSetting, CONTENT_SETTING_ALLOW, true},
    {prefs::kDefaultJavaScriptSetting, CONTENT_SETTING_ALLOW, true},
    {prefs::kDefaultPluginsSetting, CONTENT_SETTING_ALLOW, true},
    {prefs::kDefaultPopupsSetting, CONTENT_SETTING_BLOCK, true},
    {prefs::kDefaultGeolocationSetting, CONTENT_SETTING_ASK, false},
    {prefs::kDefaultNotificationsSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultAutoSelectCertificateSetting, CONTENT_SETTING_DEFAULT,
        false},
    {prefs::kDefaultFullScreenSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultMouseLockSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultMixedScriptSetting, CONTENT_SETTING_DEFAULT, true},
    {prefs::kDefaultMediaStreamSetting, CONTENT_SETTING_ASK, false},
    {prefs::kDefaultMediaStreamMicSetting, CONTENT_SETTING_ASK, false},
    {prefs::kDefaultMediaStreamCameraSetting, CONTENT_SETTING_ASK, false},
    {prefs::kDefaultProtocolHandlersSetting, CONTENT_SETTING_DEFAULT, true},
    {prefs::kDefaultPpapiBrokerSetting, CONTENT_SETTING_ASK, false},
    {prefs::kDefaultAutomaticDownloadsSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultMidiSysexSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultPushMessagingSetting, CONTENT_SETTING_ASK, true},
    {prefs::kDefaultSSLCertDecisionsSetting, CONTENT_SETTING_ALLOW, false},
#if defined(OS_WIN)
    {prefs::kDefaultMetroSwitchToDesktopSetting, CONTENT_SETTING_ASK, true},
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
    {prefs::kDefaultProtectedMediaIdentifierSetting, CONTENT_SETTING_ASK,
        false},
#endif
    {prefs::kDefaultAppBannerSetting, CONTENT_SETTING_DEFAULT, false}
};
static_assert(arraysize(kDefaultSettings) == CONTENT_SETTINGS_NUM_TYPES,
              "kDefaultSettings should have CONTENT_SETTINGS_NUM_TYPES "
              "elements");

}  // namespace

namespace content_settings {

namespace {

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
  // The registration of the preference prefs::kDefaultContentSettings should
  // also include the default values for default content settings. This allows
  // functional tests to get default content settings by reading the preference
  // prefs::kDefaultContentSettings via pyauto.
  // TODO(markusheintz): Write pyauto hooks for the content settings map as
  // content settings should be read from the host content settings map.
  base::DictionaryValue* default_content_settings = new base::DictionaryValue();
  registry->RegisterDictionaryPref(
      prefs::kDefaultContentSettings,
      default_content_settings,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Register individual default setting preferences.
  // TODO(msramek): The aggregate preference above is deprecated. Remove it
  // after two stable releases.
  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    registry->RegisterIntegerPref(
        kDefaultSettings[i].pref_name,
        kDefaultSettings[i].default_value,
        kDefaultSettings[i].syncable
            ? user_prefs::PrefRegistrySyncable::SYNCABLE_PREF
            : user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  }

  // Whether the deprecated dictionary preference has already been migrated
  // into the individual preferences in this profile.
  registry->RegisterBooleanPref(
      prefs::kMigratedDefaultContentSettings,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Whether the deprecated mediastream default setting has already been
  // migrated into microphone and camera default settings.
  registry->RegisterBooleanPref(
      prefs::kMigratedDefaultMediaStreamSetting,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

DefaultProvider::DefaultProvider(PrefService* prefs, bool incognito)
    : prefs_(prefs),
      is_incognito_(incognito),
      updating_preferences_(false) {
  DCHECK(prefs_);

  // Migrate the dictionary of default content settings to the new individual
  // preferences.
  MigrateDefaultSettings();

  // Migrate the obsolete media stream default setting into the new microphone
  // and camera settings.
  MigrateObsoleteMediaContentSetting();

  // Read global defaults.
  ReadDefaultSettings();

  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultCookiesSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_COOKIES].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultImagesSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_IMAGES].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultJavaScriptSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_JAVASCRIPT].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPluginsSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_PLUGINS].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPopupsSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_POPUPS].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultLocationSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_GEOLOCATION].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultNotificationsSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_NOTIFICATIONS].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMouseCursorSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_MOUSELOCK].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMediaStreamSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_MEDIASTREAM].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultMIDISysExSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_MIDI_SYSEX].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.DefaultPushMessagingSetting",
      IntToContentSetting(prefs_->GetInteger(
          kDefaultSettings[CONTENT_SETTINGS_TYPE_PUSH_MESSAGING].pref_name)),
      CONTENT_SETTING_NUM_SETTINGS);

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::Bind(
      &DefaultProvider::OnPreferenceChanged, base::Unretained(this));
  pref_change_registrar_.Add(prefs::kDefaultContentSettings, callback);

  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    pref_change_registrar_.Add(kDefaultSettings[i].pref_name, callback);
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
    base::AutoLock lock(lock_);

    ChangeSetting(content_type, value.get());
    WriteIndividualPref(content_type, value.get());

    // If the changed setting is syncable, write it to the old dictionary
    // preference as well, so it can be synced to older versions of Chrome.
    // TODO(msramek): Remove this after two stable releases.
    if (kDefaultSettings[content_type].syncable)
      WriteDictionaryPref(content_type, value.get());
  }

  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  content_type,
                  std::string());

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
  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType content_type = ContentSettingsType(i);
    ChangeSetting(content_type, ReadIndividualPref(content_type).get());
  }
}

bool DefaultProvider::IsValueEmptyOrDefault(ContentSettingsType content_type,
                                       base::Value* value) {
  return (value == NULL ||
          ValueToContentSetting(value)
              == kDefaultSettings[content_type].default_value);
}

void DefaultProvider::ChangeSetting(ContentSettingsType content_type,
                                    base::Value* value) {
  if (!value) {
    default_settings_[content_type].reset(
        kDefaultSettings[content_type].default_value != CONTENT_SETTING_DEFAULT
            ? new base::FundamentalValue(
                kDefaultSettings[content_type].default_value)
            : NULL);
  } else {
    default_settings_[content_type].reset(value->DeepCopy());
  }
}

void DefaultProvider::WriteIndividualPref(ContentSettingsType content_type,
                                          base::Value* value) {
  if (IsValueEmptyOrDefault(content_type, value)) {
    prefs_->ClearPref(kDefaultSettings[content_type].pref_name);
    return;
  }

  int int_value = kDefaultSettings[content_type].default_value;
  bool is_integer = value->GetAsInteger(&int_value);
  DCHECK(is_integer);
  prefs_->SetInteger(kDefaultSettings[content_type].pref_name, int_value);
}

void DefaultProvider::WriteDictionaryPref(ContentSettingsType content_type,
                                          base::Value* value) {
  // |DefaultProvider| should not send any notifications when holding
  // |lock_|. |DictionaryPrefUpdate| destructor and
  // |PrefService::SetInteger()| send out notifications. As a response, the
  // upper layers may call |GetAllContentSettingRules| which acquires |lock_|
  // again.
  DictionaryPrefUpdate update(prefs_, prefs::kDefaultContentSettings);
  base::DictionaryValue* default_settings_dictionary = update.Get();

  if (IsValueEmptyOrDefault(content_type, value)) {
    default_settings_dictionary->RemoveWithoutPathExpansion(
        GetTypeName(content_type), NULL);
    return;
  }

  default_settings_dictionary->SetWithoutPathExpansion(
      GetTypeName(content_type), value->DeepCopy());
}

void DefaultProvider::OnPreferenceChanged(const std::string& name) {
  DCHECK(CalledOnValidThread());
  if (updating_preferences_)
    return;

  // Write the changed setting from individual preferences to dictionary,
  // or vice versa - depending on which of them changed.
  // TODO(msramek): This is only necessary in the phase of migration between
  // the old dictionary preference and the new individual preferences. Remove
  // this after two stable releases.
  std::vector<ContentSettingsType> to_notify;

  if (name == prefs::kDefaultContentSettings) {
    // If the dictionary preference gets synced from an old version
    // of Chrome, we should update all individual preferences that
    // are marked as syncable.
    base::AutoLock lock(lock_);
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);

    scoped_ptr<ValueMap> dictionary = ReadDictionaryPref();

    for (ValueMap::iterator it = dictionary->begin();
         it != dictionary->end(); ++it) {
      if (!kDefaultSettings[it->first].syncable)
        continue;

      DCHECK(default_settings_.find(it->first) != default_settings_.end());
      ChangeSetting(it->first, it->second.get());
      WriteIndividualPref(it->first, it->second.get());
      to_notify.push_back(it->first);
    }
  } else {
    // Find out which content setting the preference corresponds to.
    ContentSettingsType content_type = CONTENT_SETTINGS_TYPE_DEFAULT;

    for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
      if (kDefaultSettings[i].pref_name == name) {
        content_type = ContentSettingsType(i);
        break;
      }
    }

    if (content_type == CONTENT_SETTINGS_TYPE_DEFAULT) {
      NOTREACHED() << "Unexpected preference observed";
      return;
    }

    // A new individual preference is changed. If it is syncable, we should
    // change its entry in the dictionary preference as well, so that it
    // can be synced to older versions of Chrome.
    base::AutoLock lock(lock_);
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);

    ChangeSetting(content_type, ReadIndividualPref(content_type).get());
    if (kDefaultSettings[content_type].syncable)
      WriteDictionaryPref(content_type, default_settings_[content_type].get());
    to_notify.push_back(content_type);
  }

  for (size_t i = 0; i < to_notify.size(); ++i) {
    NotifyObservers(ContentSettingsPattern(),
                    ContentSettingsPattern(),
                    to_notify[i],
                    std::string());
  }
}

scoped_ptr<base::Value> DefaultProvider::ReadIndividualPref(
    ContentSettingsType content_type) {
  int int_value = prefs_->GetInteger(kDefaultSettings[content_type].pref_name);

  if (int_value == CONTENT_SETTING_DEFAULT)
    return make_scoped_ptr((base::Value*)NULL);
  else
    return make_scoped_ptr(new base::FundamentalValue(int_value));
}

scoped_ptr<DefaultProvider::ValueMap> DefaultProvider::ReadDictionaryPref() {
  const base::DictionaryValue* default_settings_dictionary =
      prefs_->GetDictionary(prefs::kDefaultContentSettings);

  scoped_ptr<ValueMap> value_map =
      GetSettingsFromDictionary(default_settings_dictionary);

  ForceDefaultsToBeExplicit(value_map.get());
  return value_map.Pass();
}

void DefaultProvider::ForceDefaultsToBeExplicit(ValueMap* value_map) {
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType type = ContentSettingsType(i);
    if (!(*value_map)[type].get() &&
        kDefaultSettings[i].default_value != CONTENT_SETTING_DEFAULT) {
      (*value_map)[type].reset(
          new base::FundamentalValue(kDefaultSettings[i].default_value));
    }
  }
}

scoped_ptr<DefaultProvider::ValueMap>
    DefaultProvider::GetSettingsFromDictionary(
        const base::DictionaryValue* dictionary) {
  if (!dictionary)
    return make_scoped_ptr(new ValueMap());
  ValueMap* value_map = new ValueMap();

  for (base::DictionaryValue::Iterator i(*dictionary);
       !i.IsAtEnd(); i.Advance()) {
    const std::string& content_type(i.key());
    for (size_t type = 0; type < CONTENT_SETTINGS_NUM_TYPES; ++type) {
      if (content_type == GetTypeName(ContentSettingsType(type))) {
        int int_value = CONTENT_SETTING_DEFAULT;
        bool is_integer = i.value().GetAsInteger(&int_value);
        DCHECK(is_integer);
        (*value_map)[ContentSettingsType(type)].reset(
            new base::FundamentalValue(int_value));
        break;
      }
    }
  }
  // Migrate obsolete cookie prompt mode.
  if (ValueToContentSetting(
      (*value_map)[CONTENT_SETTINGS_TYPE_COOKIES].get())
      == CONTENT_SETTING_ASK) {
    (*value_map)[CONTENT_SETTINGS_TYPE_COOKIES].reset(
        new base::FundamentalValue(CONTENT_SETTING_BLOCK));
  }

  return make_scoped_ptr(value_map);
}

void DefaultProvider::MigrateDefaultSettings() {
  // Only do the migration once.
  if (prefs_->GetBoolean(prefs::kMigratedDefaultContentSettings))
    return;

  scoped_ptr<DefaultProvider::ValueMap> value_map = ReadDictionaryPref();

  for (size_t i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i) {
    ContentSettingsType content_type = ContentSettingsType(i);
    WriteIndividualPref(content_type, (*value_map)[content_type].get());
  }

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // For protected media identifier, it is desirable to just reset the setting
  // instead of migrating it from the old preference.
  WriteIndividualPref(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, NULL);
#endif

  prefs_->SetBoolean(prefs::kMigratedDefaultContentSettings, true);
}

void DefaultProvider::MigrateObsoleteMediaContentSetting() {
  // We only do the migration once.
  if (prefs_->GetBoolean(prefs::kMigratedDefaultMediaStreamSetting))
    return;

  scoped_ptr<base::Value> value = ReadIndividualPref(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM);
  WriteIndividualPref(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, value.get());
  WriteIndividualPref(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, value.get());
  WriteIndividualPref(CONTENT_SETTINGS_TYPE_MEDIASTREAM, NULL);

  prefs_->SetBoolean(prefs::kMigratedDefaultMediaStreamSetting, true);
}

}  // namespace content_settings
