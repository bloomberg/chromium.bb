// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/url_formatter/url_fixer.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace extensions {

namespace settings_private = api::settings_private;

PrefsUtil::PrefsUtil(Profile* profile) : profile_(profile) {
}

PrefsUtil::~PrefsUtil() {
}

#if defined(OS_CHROMEOS)
using CrosSettings = chromeos::CrosSettings;
#endif

const PrefsUtil::TypedPrefMap& PrefsUtil::GetWhitelistedKeys() {
  static PrefsUtil::TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new PrefsUtil::TypedPrefMap();
  (*s_whitelist)["alternate_error_pages.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["bookmark_bar.show_on_all_tabs"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["browser.show_home_button"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["download.default_directory"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["download.prompt_for_download"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["enable_do_not_track"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["homepage"] = settings_private::PrefType::PREF_TYPE_URL;
  (*s_whitelist)["net.network_prediction_options"] =
      settings_private::PrefType::PREF_TYPE_NUMBER;
  (*s_whitelist)["safebrowsing.enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["safebrowsing.extended_reporting_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["search.suggest_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["spellcheck.use_spelling_service"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;

#if defined(OS_CHROMEOS)
  (*s_whitelist)["cros.accounts.allowBWSI"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.supervisedUsersEnabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.showUserNamesOnSignIn"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.allowGuest"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.accounts.users"] =
      settings_private::PrefType::PREF_TYPE_LIST;
  (*s_whitelist)["settings.accessibility"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.autoclick"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.autoclick_delay_ms"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.enable_menu"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.high_contrast_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.large_cursor_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.screen_magnifier"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.sticky_keys_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.a11y.virtual_keyboard"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.clock.use_24hour_clock"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.touchpad.enable_tap_dragging"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.metrics.reportingEnabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["cros.device.attestation_for_content_protection_enabled"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["settings.internet.wake_on_wifi_ssid"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
#endif

  return *s_whitelist;
}

api::settings_private::PrefType PrefsUtil::GetType(const std::string& name,
                                                   base::Value::Type type) {
  switch (type) {
    case base::Value::Type::TYPE_BOOLEAN:
      return api::settings_private::PrefType::PREF_TYPE_BOOLEAN;
    case base::Value::Type::TYPE_INTEGER:
    case base::Value::Type::TYPE_DOUBLE:
      return api::settings_private::PrefType::PREF_TYPE_NUMBER;
    case base::Value::Type::TYPE_STRING:
      return IsPrefTypeURL(name)
                 ? api::settings_private::PrefType::PREF_TYPE_URL
                 : api::settings_private::PrefType::PREF_TYPE_STRING;
    case base::Value::Type::TYPE_LIST:
      return api::settings_private::PrefType::PREF_TYPE_LIST;
    default:
      return api::settings_private::PrefType::PREF_TYPE_NONE;
  }
}

scoped_ptr<api::settings_private::PrefObject> PrefsUtil::GetCrosSettingsPref(
    const std::string& name) {
  scoped_ptr<api::settings_private::PrefObject> pref_object(
      new api::settings_private::PrefObject());

#if defined(OS_CHROMEOS)
  const base::Value* value = CrosSettings::Get()->GetPref(name);
  pref_object->key = name;
  pref_object->type = GetType(name, value->GetType());
  pref_object->value.reset(value->DeepCopy());
#endif

  return pref_object.Pass();
}

scoped_ptr<api::settings_private::PrefObject> PrefsUtil::GetPref(
    const std::string& name) {
  if (IsCrosSetting(name))
    return GetCrosSettingsPref(name);

  PrefService* pref_service = FindServiceForPref(name);
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref)
    return nullptr;

  scoped_ptr<api::settings_private::PrefObject> pref_object(
      new api::settings_private::PrefObject());
  pref_object->key = pref->name();
  pref_object->type = GetType(name, pref->GetType());
  pref_object->value.reset(pref->GetValue()->DeepCopy());

  if (pref->IsManaged()) {
    if (pref->IsManagedByCustodian()) {
      pref_object->policy_source =
          api::settings_private::PolicySource::POLICY_SOURCE_DEVICE;
    } else {
      pref_object->policy_source =
          api::settings_private::PolicySource::POLICY_SOURCE_USER;
    }
    pref_object->policy_enforcement =
        pref->IsRecommended() ? api::settings_private::PolicyEnforcement::
                                    POLICY_ENFORCEMENT_RECOMMENDED
                              : api::settings_private::PolicyEnforcement::
                                    POLICY_ENFORCEMENT_ENFORCED;
  } else if (!IsPrefUserModifiable(name)) {
    pref_object->policy_source =
        api::settings_private::PolicySource::POLICY_SOURCE_USER;
    pref_object->policy_enforcement =
        api::settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
  }

  return pref_object.Pass();
}

bool PrefsUtil::SetPref(const std::string& pref_name,
                        const base::Value* value) {
  if (IsCrosSetting(pref_name))
    return SetCrosSettingsPref(pref_name, value);

  PrefService* pref_service = FindServiceForPref(pref_name);

  if (!IsPrefUserModifiable(pref_name))
    return false;

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name);
  if (!pref)
    return false;

  DCHECK_EQ(pref->GetType(), value->GetType());

  switch (pref->GetType()) {
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_DOUBLE:
    case base::Value::TYPE_LIST:
      pref_service->Set(pref_name, *value);
      break;
    case base::Value::TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      if (!value->GetAsDouble(&double_value))
        return false;

      pref_service->SetInteger(pref_name, static_cast<int>(double_value));
      break;
    }
    case base::Value::TYPE_STRING: {
      std::string string_value;
      if (!value->GetAsString(&string_value))
        return false;

      if (IsPrefTypeURL(pref_name)) {
        GURL fixed = url_formatter::FixupURL(string_value, std::string());
        string_value = fixed.spec();
      }

      pref_service->SetString(pref_name, string_value);
      break;
    }
    default:
      return false;
  }

  // TODO(orenb): Process setting metrics here and in the CrOS setting method
  // too (like "ProcessUserMetric" in CoreOptionsHandler).
  return true;
}

bool PrefsUtil::SetCrosSettingsPref(const std::string& pref_name,
                                    const base::Value* value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name))
    return service->Set(pref_name, *value);

  chromeos::CrosSettings::Get()->Set(pref_name, *value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::AppendToListCrosSetting(const std::string& pref_name,
                                        const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->AppendToList(pref_name, value);
  }

  chromeos::CrosSettings::Get()->AppendToList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::RemoveFromListCrosSetting(const std::string& pref_name,
                                          const base::Value& value) {
#if defined(OS_CHROMEOS)
  chromeos::OwnerSettingsServiceChromeOS* service =
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile_);

  // Returns false if not the owner, for settings requiring owner.
  if (service && service->HandlesSetting(pref_name)) {
    return service->RemoveFromList(pref_name, value);
  }

  chromeos::CrosSettings::Get()->RemoveFromList(pref_name, &value);
  return true;
#else
  return false;
#endif
}

bool PrefsUtil::IsPrefTypeURL(const std::string& pref_name) {
  settings_private::PrefType pref_type =
      settings_private::PrefType::PREF_TYPE_NONE;

  const TypedPrefMap keys = GetWhitelistedKeys();
  const auto& iter = keys.find(pref_name);
  if (iter != keys.end())
    pref_type = iter->second;

  return pref_type == settings_private::PrefType::PREF_TYPE_URL;
}

bool PrefsUtil::IsPrefUserModifiable(const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return true;
  }

  PrefService* pref_service = profile_->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref || !pref->IsUserModifiable() || profile_->IsSupervised())
    return false;

  return true;
}

PrefService* PrefsUtil::FindServiceForPref(const std::string& pref_name) {
  PrefService* user_prefs = profile_->GetPrefs();

  // Proxy is a peculiar case: on ChromeOS, settings exist in both user
  // prefs and local state, but chrome://settings should affect only user prefs.
  // Elsewhere the proxy settings are stored in local state.
  // See http://crbug.com/157147

  if (pref_name == prefs::kProxy) {
#if defined(OS_CHROMEOS)
    return user_prefs;
#else
    return g_browser_process->local_state();
#endif
  }

  // Find which PrefService contains the given pref. Pref names should not
  // be duplicated across services, however if they are, prefer the user's
  // prefs.
  if (user_prefs->FindPreference(pref_name))
    return user_prefs;

  if (g_browser_process->local_state()->FindPreference(pref_name))
    return g_browser_process->local_state();

  return user_prefs;
}

bool PrefsUtil::IsCrosSetting(const std::string& pref_name) {
#if defined(OS_CHROMEOS)
  return CrosSettings::Get()->IsCrosSettings(pref_name);
#else
  return false;
#endif
}

}  // namespace extensions
