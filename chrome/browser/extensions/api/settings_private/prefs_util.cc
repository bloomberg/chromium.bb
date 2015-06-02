// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace extensions {

namespace settings_private = api::settings_private;

namespace prefs_util {

const TypedPrefMap& GetWhitelistedKeys() {
  static TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new TypedPrefMap();
  (*s_whitelist)["browser.show_home_button"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["bookmark_bar.show_on_all_tabs"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["download.default_directory"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["download.prompt_for_download"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["homepage"] = settings_private::PrefType::PREF_TYPE_URL;

#if defined(OS_CHROMEOS)
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
#endif

  return *s_whitelist;
}

scoped_ptr<api::settings_private::PrefObject> GetPref(Profile* profile,
                                                      const std::string& name) {
  scoped_ptr<api::settings_private::PrefObject> pref_object(
      new api::settings_private::PrefObject());

  PrefService* pref_service = FindServiceForPref(profile, name);
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref)
    return pref_object.Pass();

  pref_object->key = pref->name();
  switch (pref->GetType()) {
    case base::Value::Type::TYPE_BOOLEAN:
      pref_object->type = api::settings_private::PrefType::PREF_TYPE_BOOLEAN;
      break;
    case base::Value::Type::TYPE_INTEGER:
    case base::Value::Type::TYPE_DOUBLE:
      pref_object->type = api::settings_private::PrefType::PREF_TYPE_NUMBER;
      break;
    case base::Value::Type::TYPE_STRING:
      pref_object->type =
          IsPrefTypeURL(name)
              ? api::settings_private::PrefType::PREF_TYPE_URL
              : api::settings_private::PrefType::PREF_TYPE_STRING;
      break;
    case base::Value::Type::TYPE_LIST:
      pref_object->type = api::settings_private::PrefType::PREF_TYPE_LIST;
      break;
    default:
      break;
  }

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
  } else if (!IsPrefUserModifiable(profile, name)) {
    pref_object->policy_source =
        api::settings_private::PolicySource::POLICY_SOURCE_USER;
    pref_object->policy_enforcement =
        api::settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
  }

  return pref_object.Pass();
}

bool IsPrefTypeURL(const std::string& pref_name) {
  settings_private::PrefType pref_type =
      settings_private::PrefType::PREF_TYPE_NONE;

  const TypedPrefMap keys = GetWhitelistedKeys();
  const auto& iter = keys.find(pref_name);
  if (iter != keys.end())
    pref_type = iter->second;

  return pref_type == settings_private::PrefType::PREF_TYPE_URL;
}

bool IsPrefUserModifiable(Profile* profile, const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return true;
  }

  PrefService* pref_service = profile->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref || !pref->IsUserModifiable() || profile->IsSupervised())
    return false;

  return true;
}

PrefService* FindServiceForPref(Profile* profile,
                                const std::string& pref_name) {
  PrefService* user_prefs = profile->GetPrefs();

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

}  // namespace prefs_util

}  // namespace extensions
