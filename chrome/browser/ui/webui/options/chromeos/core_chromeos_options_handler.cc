// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

#include <string>

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

namespace {

// Create a settings value with "managed" and "disabled" property.
// "managed" property is true if the setting is managed by administrator.
// "disabled" property is true if the UI for the setting should be disabled.
base::Value* CreateSettingsValue(base::Value *value,
                                 bool managed,
                                 bool disabled) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", value);
  dict->Set("managed", base::Value::CreateBooleanValue(managed));
  dict->Set("disabled", base::Value::CreateBooleanValue(disabled));
  return dict;
}

// Returns true if |username| is the logged-in owner.
bool IsLoggedInOwner(const std::string& username) {
  UserManager* user_manager = UserManager::Get();
  return user_manager->current_user_is_owner() &&
      user_manager->logged_in_user().email() == username;
}

// Creates a user info dictionary to be stored in the |ListValue| that is
// passed to Javascript for the |kAccountsPrefUsers| preference.
base::DictionaryValue* CreateUserInfo(const std::string& username,
                                      const std::string& display_email,
                                      const std::string& display_name) {
  base::DictionaryValue* user_dict = new DictionaryValue;
  user_dict->SetString("username", username);
  user_dict->SetString("name", display_email);
  user_dict->SetString("email", display_name);
  user_dict->SetBoolean("owner", IsLoggedInOwner(username));
  return user_dict;
}

// This function decorates the bare list of emails with some more information
// needed by the UI to properly display the Accounts page.
base::Value* CreateUsersWhitelist(const base::Value *pref_value) {
  const base::ListValue* list_value =
      static_cast<const base::ListValue*>(pref_value);
  base::ListValue* user_list = new base::ListValue();
  UserManager* user_manager = UserManager::Get();

  for (base::ListValue::const_iterator i = list_value->begin();
       i != list_value->end(); ++i) {
    std::string email;
    if ((*i)->GetAsString(&email)) {
      // Translate email to the display email.
      std::string display_email = user_manager->GetUserDisplayEmail(email);
      // TODO(ivankr): fetch display name for existing users.
      user_list->Append(CreateUserInfo(email, display_email, std::string()));
    }
  }
  return user_list;
}

}  // namespace

CoreChromeOSOptionsHandler::CoreChromeOSOptionsHandler()
    : handling_change_(false),
      pointer_factory_(this) {
}

CoreChromeOSOptionsHandler::~CoreChromeOSOptionsHandler() {
  PrefProxyConfigTracker* proxy_tracker =
      Profile::FromWebUI(web_ui_)->GetProxyConfigTracker();
  proxy_tracker->RemoveNotificationCallback(
      base::Bind(&CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged,
                 pointer_factory_.GetWeakPtr()));
}

void CoreChromeOSOptionsHandler::Initialize() {
  proxy_prefs_.reset(PrefSetObserver::CreateProxyPrefSetObserver(
    Profile::FromWebUI(web_ui_)->GetPrefs(), this));
  // Observe the chromeos::ProxyConfigServiceImpl for changes from the UI.
  PrefProxyConfigTracker* proxy_tracker =
      Profile::FromWebUI(web_ui_)->GetProxyConfigTracker();
  proxy_tracker->AddNotificationCallback(
      base::Bind(&CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged,
                 pointer_factory_.GetWeakPtr()));
}

base::Value* CoreChromeOSOptionsHandler::FetchPref(
    const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    base::Value *value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(Profile::FromWebUI(web_ui_),
                                                  pref_name, &value);
    if (!value)
      return base::Value::CreateNullValue();

    return value;
  }
  if (!CrosSettings::IsCrosSettings(pref_name)) {
    // Specially handle kUseSharedProxies because kProxy controls it to
    // determine if it's managed by policy/extension.
    if (pref_name == prefs::kUseSharedProxies) {
      PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();
      const PrefService::Preference* pref =
          pref_service->FindPreference(prefs::kUseSharedProxies);
      if (!pref)
        return base::Value::CreateNullValue();
      const PrefService::Preference* controlling_pref =
          pref_service->FindPreference(prefs::kProxy);
      return CreateValueForPref(pref, controlling_pref);
    }
    return ::CoreOptionsHandler::FetchPref(pref_name);
  }

  const base::Value* pref_value = CrosSettings::Get()->GetPref(pref_name);
  if (!pref_value)
    return base::Value::CreateNullValue();

  // Lists don't get the standard pref decoration.
  if (pref_value->GetType() == base::Value::TYPE_LIST) {
    if (pref_name == kAccountsPrefUsers)
      return CreateUsersWhitelist(pref_value);
    // Return a copy because the UI will take ownership of this object.
    return pref_value->DeepCopy();
  }
  // All other prefs are decorated the same way.
  return CreateSettingsValue(
      pref_value->DeepCopy(),  // The copy will be owned by the dictionary.
      g_browser_process->browser_policy_connector()->IsEnterpriseManaged(),
      !UserManager::Get()->current_user_is_owner());
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    // We observe those all the time.
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::ObservePref(pref_name);
  CrosSettings::Get()->AddSettingsObserver(pref_name.c_str(), this);
}

void CoreChromeOSOptionsHandler::SetPref(const std::string& pref_name,
                                         const base::Value* value,
                                         const std::string& metric) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    proxy_cros_settings_parser::SetProxyPrefValue(Profile::FromWebUI(web_ui_),
                                                  pref_name, value);
    ProcessUserMetric(value, metric);
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::SetPref(pref_name, value, metric);
  handling_change_ = true;
  CrosSettings::Get()->Set(pref_name, *value);
  handling_change_ = false;

  ProcessUserMetric(value, metric);
}

void CoreChromeOSOptionsHandler::StopObservingPref(const std::string& path) {
  if (proxy_cros_settings_parser::IsProxyPref(path))
    return;  // We unregister those in the destructor.
  // Unregister this instance from observing prefs of chrome os settings.
  if (CrosSettings::IsCrosSettings(path))
    CrosSettings::Get()->RemoveSettingsObserver(path.c_str(), this);
  else  // Call base class to handle regular preferences.
    ::CoreOptionsHandler::StopObservingPref(path);
}

void CoreChromeOSOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Ignore the notification if this instance had caused it.
  if (handling_change_)
    return;
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED) {
    NotifySettingsChanged(content::Details<std::string>(details).ptr());
    return;
  }
  // Special handling for preferences kUseSharedProxies and kProxy, the latter
  // controls the former and decides if it's managed by policy/extension.
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    const PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();
    std::string* pref_name = content::Details<std::string>(details).ptr();
    if (content::Source<PrefService>(source).ptr() == pref_service &&
        (proxy_prefs_->IsObserved(*pref_name) ||
         *pref_name == prefs::kUseSharedProxies)) {
      NotifyPrefChanged(prefs::kUseSharedProxies, prefs::kProxy);
      return;
    }
  }
  ::CoreOptionsHandler::Observe(type, source, details);
}

void CoreChromeOSOptionsHandler::NotifySettingsChanged(
    const std::string* setting_name) {
  DCHECK(web_ui_);
  DCHECK(CrosSettings::Get()->IsCrosSettings(*setting_name));
  const base::Value* value = FetchPref(*setting_name);
  if (!value) {
    NOTREACHED();
    return;
  }
  for (PreferenceCallbackMap::const_iterator iter =
      pref_callback_map_.find(*setting_name);
      iter != pref_callback_map_.end(); ++iter) {
    const std::wstring& callback_function = iter->second;
    ListValue result_value;
    result_value.Append(base::Value::CreateStringValue(setting_name->c_str()));
    result_value.Append(value->DeepCopy());
    web_ui_->CallJavascriptFunction(WideToASCII(callback_function),
                                    result_value);
  }
  if (value)
    delete value;
}

void CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged() {
  DCHECK(web_ui_);
  for (size_t i = 0; i < kProxySettingsCount; ++i) {
    base::Value* value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(
        Profile::FromWebUI(web_ui_), kProxySettings[i], &value);
    DCHECK(value);
    PreferenceCallbackMap::const_iterator iter =
        pref_callback_map_.find(kProxySettings[i]);
    for (; iter != pref_callback_map_.end(); ++iter) {
      const std::wstring& callback_function = iter->second;
      ListValue result_value;
      result_value.Append(base::Value::CreateStringValue(kProxySettings[i]));
      result_value.Append(value->DeepCopy());
      web_ui_->CallJavascriptFunction(WideToASCII(callback_function),
                                      result_value);
    }
    if (value)
      delete value;
  }
}

}  // namespace chromeos
