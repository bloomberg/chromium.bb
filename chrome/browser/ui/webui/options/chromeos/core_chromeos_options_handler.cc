// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

#include <string>

#include "base/bind.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {
namespace options {

namespace {

// List of settings that should be changeable by all users.
const char* kNonOwnerSettings[] = {
    kSystemTimezone
};

// Returns true if |pref| should be only available to the owner.
bool IsSettingOwnerOnly(const std::string& pref) {
  const char** end = kNonOwnerSettings + arraysize(kNonOwnerSettings);
  return std::find(kNonOwnerSettings, end, pref) == end;
}

// Returns true if |username| is the logged-in owner.
bool IsLoggedInOwner(const std::string& username) {
  UserManager* user_manager = UserManager::Get();
  return user_manager->IsCurrentUserOwner() &&
      user_manager->GetLoggedInUser()->email() == username;
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
    : pointer_factory_(this) {
}

CoreChromeOSOptionsHandler::~CoreChromeOSOptionsHandler() {
  PrefProxyConfigTracker* proxy_tracker =
      Profile::FromWebUI(web_ui())->GetProxyConfigTracker();
  proxy_tracker->RemoveNotificationCallback(
      base::Bind(&CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged,
                 pointer_factory_.GetWeakPtr()));
}

void CoreChromeOSOptionsHandler::InitializeHandler() {
  CoreOptionsHandler::InitializeHandler();

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  proxy_prefs_.Init(prefs);
  proxy_prefs_.Add(prefs::kProxy,
                   base::Bind(&CoreChromeOSOptionsHandler::OnPreferenceChanged,
                              base::Unretained(this),
                              prefs));
  // Observe the chromeos::ProxyConfigServiceImpl for changes from the UI.
  PrefProxyConfigTracker* proxy_tracker =
      Profile::FromWebUI(web_ui())->GetProxyConfigTracker();
  proxy_tracker->AddNotificationCallback(
      base::Bind(&CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged,
                 pointer_factory_.GetWeakPtr()));
}

base::Value* CoreChromeOSOptionsHandler::FetchPref(
    const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    base::Value *value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(Profile::FromWebUI(web_ui()),
                                                  pref_name, &value);
    if (!value)
      return base::Value::CreateNullValue();

    return value;
  }
  if (!CrosSettings::IsCrosSettings(pref_name)) {
    std::string controlling_pref =
        pref_name == prefs::kUseSharedProxies ? prefs::kProxy : std::string();
    return CreateValueForPref(pref_name, controlling_pref);
  }

  const base::Value* pref_value = CrosSettings::Get()->GetPref(pref_name);
  if (!pref_value)
    return base::Value::CreateNullValue();

  // Decorate pref value as CoreOptionsHandler::CreateValueForPref() does.
  // TODO(estade): seems that this should replicate CreateValueForPref less.
  DictionaryValue* dict = new DictionaryValue;
  if (pref_name == kAccountsPrefUsers)
    dict->Set("value", CreateUsersWhitelist(pref_value));
  else
    dict->Set("value", pref_value->DeepCopy());
  if (g_browser_process->browser_policy_connector()->IsEnterpriseManaged())
    dict->SetString("controlledBy", "policy");
  dict->SetBoolean("disabled",
                   IsSettingOwnerOnly(pref_name) &&
                       !UserManager::Get()->IsCurrentUserOwner());
  return dict;
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    // We observe those all the time.
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::options::CoreOptionsHandler::ObservePref(pref_name);
  CrosSettings::Get()->AddSettingsObserver(pref_name.c_str(), this);
}

void CoreChromeOSOptionsHandler::SetPref(const std::string& pref_name,
                                         const base::Value* value,
                                         const std::string& metric) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    proxy_cros_settings_parser::SetProxyPrefValue(Profile::FromWebUI(web_ui()),
                                                  pref_name, value);
    base::StringValue proxy_type(pref_name);
    web_ui()->CallJavascriptFunction(
        "options.internet.DetailsInternetPage.updateProxySettings",
        proxy_type);
    ProcessUserMetric(value, metric);
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::options::CoreOptionsHandler::SetPref(pref_name, value, metric);
  CrosSettings::Get()->Set(pref_name, *value);

  ProcessUserMetric(value, metric);
}

void CoreChromeOSOptionsHandler::StopObservingPref(const std::string& path) {
  if (proxy_cros_settings_parser::IsProxyPref(path))
    return;  // We unregister those in the destructor.
  // Unregister this instance from observing prefs of chrome os settings.
  if (CrosSettings::IsCrosSettings(path))
    CrosSettings::Get()->RemoveSettingsObserver(path.c_str(), this);
  else  // Call base class to handle regular preferences.
    ::options::CoreOptionsHandler::StopObservingPref(path);
}

void CoreChromeOSOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  CoreOptionsHandler::GetLocalizedValues(localized_strings);

  AddAccountUITweaksLocalizedValues(localized_strings);
}

void CoreChromeOSOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED) {
    NotifySettingsChanged(content::Details<std::string>(details).ptr());
    return;
  }
  ::options::CoreOptionsHandler::Observe(type, source, details);
}

void CoreChromeOSOptionsHandler::OnPreferenceChanged(
    PrefService* service,
    const std::string& pref_name) {
  // Special handling for preferences kUseSharedProxies and kProxy, the latter
  // controls the former and decides if it's managed by policy/extension.
  const PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  if (service == pref_service &&
      (proxy_prefs_.IsObserved(pref_name) ||
       pref_name == prefs::kUseSharedProxies)) {
    NotifyPrefChanged(prefs::kUseSharedProxies, prefs::kProxy);
    return;
  }
  ::options::CoreOptionsHandler::OnPreferenceChanged(service, pref_name);
}

void CoreChromeOSOptionsHandler::NotifySettingsChanged(
    const std::string* setting_name) {
  DCHECK(CrosSettings::Get()->IsCrosSettings(*setting_name));
  scoped_ptr<base::Value> value(FetchPref(*setting_name));
  if (!value.get())
    NOTREACHED();
  DispatchPrefChangeNotification(*setting_name, value.Pass());
}

void CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged() {
  for (size_t i = 0; i < kProxySettingsCount; ++i) {
    base::Value* value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(
        Profile::FromWebUI(web_ui()), kProxySettings[i], &value);
    DCHECK(value);
    scoped_ptr<base::Value> ptr(value);
    DispatchPrefChangeNotification(kProxySettings[i], ptr.Pass());
  }
}

}  // namespace options
}  // namespace chromeos
