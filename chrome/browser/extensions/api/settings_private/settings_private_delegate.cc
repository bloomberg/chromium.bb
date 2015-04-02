// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_delegate.h"

#include "base/json/json_reader.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/url_fixer/url_fixer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

namespace settings_private = api::settings_private;

namespace {

// NOTE: Consider moving this function to a separate file, e.g. in
// chrome/browser/prefs.
bool IsPrefUserModifiable(Profile* profile,
                          PrefService* pref_service,
                          const std::string& pref_name) {
  if (pref_name != prefs::kBrowserGuestModeEnabled &&
      pref_name != prefs::kBrowserAddPersonEnabled) {
    return true;
  }

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref || !pref->IsUserModifiable() || profile->IsSupervised())
    return false;

  return true;
}

const TypedPrefMap& GetWhitelistedKeys() {
  static TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new TypedPrefMap();
  (*s_whitelist)["download.default_directory"] =
      settings_private::PrefType::PREF_TYPE_STRING;
  (*s_whitelist)["download.prompt_for_download"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  (*s_whitelist)["homepage"] =
      settings_private::PrefType::PREF_TYPE_URL;
  return *s_whitelist;
}

#if defined(OS_CHROMEOS)
const TypedPrefMap& GetWhitelistedCrosKeys() {
  static TypedPrefMap* s_whitelist = nullptr;
  if (s_whitelist)
    return *s_whitelist;
  s_whitelist = new TypedPrefMap();
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
  (*s_whitelist)["settings.touchpad.enable_tap_dragging"] =
      settings_private::PrefType::PREF_TYPE_BOOLEAN;
  return *s_whitelist;
}
#endif

bool IsPrefTypeURL(const std::string& pref_name) {
  settings_private::PrefType pref_type =
      settings_private::PrefType::PREF_TYPE_NONE;

  const TypedPrefMap keys = GetWhitelistedKeys();
  const auto& iter = keys.find(pref_name);
  if (iter != keys.end()) {
    pref_type = iter->second;
  }

#if defined(OS_CHROMEOS)
  const TypedPrefMap cros_keys = GetWhitelistedCrosKeys();
  const auto& cros_iter = cros_keys.find(pref_name);
  if (cros_iter != cros_keys.end()) {
    pref_type = cros_iter->second;
  }
#endif

  return pref_type == settings_private::PrefType::PREF_TYPE_URL;
}

}  // namespace

SettingsPrivateDelegate::SettingsPrivateDelegate(Profile* profile)
    : profile_(profile) {
}

SettingsPrivateDelegate::~SettingsPrivateDelegate() {
}

scoped_ptr<base::Value> SettingsPrivateDelegate::GetPref(
    const std::string& name) {
  PrefService* pref_service = profile_->GetPrefs();
  const PrefService::Preference* pref = pref_service->FindPreference(name);
  if (!pref)
    return make_scoped_ptr(base::Value::CreateNullValue());

  api::settings_private::PrefObject* pref_object =
      new api::settings_private::PrefObject();
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
      pref_object->type = IsPrefTypeURL(name)
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
  } else if (!IsPrefUserModifiable(profile_, pref_service, name)) {
    pref_object->policy_source =
        api::settings_private::PolicySource::POLICY_SOURCE_USER;
    pref_object->policy_enforcement =
        api::settings_private::PolicyEnforcement::POLICY_ENFORCEMENT_ENFORCED;
  }

  return pref_object->ToValue();
}

scoped_ptr<base::Value> SettingsPrivateDelegate::GetAllPrefs() {
  scoped_ptr<base::ListValue> prefs(new base::ListValue());

  const TypedPrefMap& keys = GetWhitelistedKeys();
  for (const auto& it : keys) {
    prefs->Append(GetPref(it.first).release());
  }

#if defined(OS_CHROMEOS)
  const TypedPrefMap cros_keys = GetWhitelistedCrosKeys();
  for (const auto& it : cros_keys) {
    prefs->Append(GetPref(it.first).release());
  }
#endif

  return prefs.Pass();
}

PrefService* SettingsPrivateDelegate::FindServiceForPref(
    const std::string& pref_name) {
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

bool SettingsPrivateDelegate::SetPref(const std::string& pref_name,
                                      const base::Value* value) {
  PrefService* pref_service = FindServiceForPref(pref_name);

  if (!IsPrefUserModifiable(profile_, pref_service, pref_name))
    return false;

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref)
    return false;

  DCHECK_EQ(pref->GetType(), value->GetType());

  scoped_ptr<base::Value> temp_value;

  switch (pref->GetType()) {
    case base::Value::TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      if (!value->GetAsDouble(&double_value))
        return false;

      int int_value = static_cast<int>(double_value);
      temp_value.reset(new base::FundamentalValue(int_value));
      value = temp_value.get();
      break;
    }
    case base::Value::TYPE_STRING: {
      std::string original;
      if (!value->GetAsString(&original))
        return false;

      if (IsPrefTypeURL(pref_name)) {
        GURL fixed = url_fixer::FixupURL(original, std::string());
        temp_value.reset(new base::StringValue(fixed.spec()));
        value = temp_value.get();
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      // In case we have a List pref we got a JSON string.
      std::string json_string;
      if (!value->GetAsString(&json_string))
        return false;

      temp_value.reset(base::JSONReader::Read(json_string));
      value = temp_value.get();
      if (!value->IsType(base::Value::TYPE_LIST))
        return false;

      break;
    }
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_DOUBLE:
      break;
    default:
      return false;
  }

  // TODO(orenb): Process setting metrics here (like "ProcessUserMetric" in
  // CoreOptionsHandler).
  pref_service->Set(pref_name.c_str(), *value);
  return true;
}

}  // namespace extensions
