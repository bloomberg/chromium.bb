// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

#include <string>

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

namespace {

// List of settings that should be changeable by all users.
const char* kNonPrivilegedSettings[] = {
    kSystemTimezone
};

// Returns true if |pref| can be controlled (e.g. by policy or owner).
bool IsSettingPrivileged(const std::string& pref) {
  const char** end = kNonPrivilegedSettings + arraysize(kNonPrivilegedSettings);
  return std::find(kNonPrivilegedSettings, end, pref) == end;
}

// Creates a user info dictionary to be stored in the |ListValue| that is
// passed to Javascript for the |kAccountsPrefUsers| preference.
base::DictionaryValue* CreateUserInfo(const std::string& username,
                                      const std::string& display_email,
                                      const std::string& display_name) {
  base::DictionaryValue* user_dict = new base::DictionaryValue;
  user_dict->SetString("username", username);
  user_dict->SetString("name", display_email);
  user_dict->SetString("email", display_name);

  bool is_owner = UserManager::Get()->GetOwnerEmail() == username;
  user_dict->SetBoolean("owner", is_owner);
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

const char kSelectNetworkMessage[] = "selectNetwork";

}  // namespace

CoreChromeOSOptionsHandler::CoreChromeOSOptionsHandler() {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED,
                              content::NotificationService::AllSources());
}

CoreChromeOSOptionsHandler::~CoreChromeOSOptionsHandler() {
}

void CoreChromeOSOptionsHandler::RegisterMessages() {
  CoreOptionsHandler::RegisterMessages();
  web_ui()->RegisterMessageCallback(
      kSelectNetworkMessage,
      base::Bind(&CoreChromeOSOptionsHandler::SelectNetworkCallback,
                 base::Unretained(this)));
}

void CoreChromeOSOptionsHandler::InitializeHandler() {
  // This function is both called on the initial page load and on each reload.
  // For the latter case, forget the last selected network.
  proxy_config_service_.SetCurrentNetwork(std::string());
  // And clear the cached configuration.
  proxy_config_service_.UpdateFromPrefs();

  CoreOptionsHandler::InitializeHandler();

  PrefService* profile_prefs = NULL;
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!ProfileHelper::IsSigninProfile(profile)) {
    profile_prefs = profile->GetPrefs();
    ObservePref(prefs::kOpenNetworkConfiguration);
  }
  ObservePref(prefs::kProxy);
  ObservePref(prefs::kDeviceOpenNetworkConfiguration);
  proxy_config_service_.SetPrefs(profile_prefs,
                                 g_browser_process->local_state());
}

void CoreChromeOSOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // The only expected notification for now is this one.
  DCHECK(type == chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED);

  // Finish this asynchronously because the notification has to tricle in to all
  // Chrome components before we can reliably read the status on the other end.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&CoreChromeOSOptionsHandler::NotifyOwnershipChanged,
                 base::Unretained(this)));
}

void CoreChromeOSOptionsHandler::NotifyOwnershipChanged() {
  for (SubscriptionMap::iterator it = pref_subscription_map_.begin();
       it != pref_subscription_map_.end(); ++it) {
    NotifySettingsChanged(it->first);
  }
}

base::Value* CoreChromeOSOptionsHandler::FetchPref(
    const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    base::Value *value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(
        proxy_config_service_, pref_name, &value);
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
  base::DictionaryValue* dict = new base::DictionaryValue;
  if (pref_name == kAccountsPrefUsers)
    dict->Set("value", CreateUsersWhitelist(pref_value));
  else
    dict->Set("value", pref_value->DeepCopy());

  std::string controlled_by;
  if (IsSettingPrivileged(pref_name)) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    if (connector->IsEnterpriseManaged())
      controlled_by = "policy";
    else if (!ProfileHelper::IsOwnerProfile(Profile::FromWebUI(web_ui())))
      controlled_by = "owner";
  }
  dict->SetBoolean("disabled", !controlled_by.empty());
  if (!controlled_by.empty())
    dict->SetString("controlledBy", controlled_by);
  return dict;
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    // We observe those all the time.
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::options::CoreOptionsHandler::ObservePref(pref_name);

  linked_ptr<CrosSettings::ObserverSubscription> subscription(
      CrosSettings::Get()->AddSettingsObserver(
          pref_name.c_str(),
          base::Bind(&CoreChromeOSOptionsHandler::NotifySettingsChanged,
                     base::Unretained(this),
                     pref_name)).release());
  pref_subscription_map_.insert(make_pair(pref_name, subscription));
}

void CoreChromeOSOptionsHandler::SetPref(const std::string& pref_name,
                                         const base::Value* value,
                                         const std::string& metric) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    proxy_cros_settings_parser::SetProxyPrefValue(
        pref_name, value, &proxy_config_service_);
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
    pref_subscription_map_.erase(path);
  else  // Call base class to handle regular preferences.
    ::options::CoreOptionsHandler::StopObservingPref(path);
}

base::Value* CoreChromeOSOptionsHandler::CreateValueForPref(
    const std::string& pref_name,
    const std::string& controlling_pref_name) {
  // The screen lock setting is shared if multiple users are logged in and at
  // least one has chosen to require passwords.
  if (pref_name == prefs::kEnableAutoScreenLock &&
      UserManager::Get()->GetLoggedInUsers().size() > 1 &&
      controlling_pref_name.empty()) {
    PrefService* user_prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    const PrefService::Preference* pref =
        user_prefs->FindPreference(prefs::kEnableAutoScreenLock);

    ash::SessionStateDelegate* delegate =
        ash::Shell::GetInstance()->session_state_delegate();
    if (pref && pref->IsUserModifiable() &&
        delegate->ShouldLockScreenBeforeSuspending()) {
      bool screen_lock = false;
      bool success = pref->GetValue()->GetAsBoolean(&screen_lock);
      DCHECK(success);
      if (!screen_lock) {
        // Screen lock is enabled for the session, but not in the user's
        // preferences. Show the user's value in the checkbox, but indicate
        // that the password requirement is enabled by some other user.
        base::DictionaryValue* dict = new base::DictionaryValue;
        dict->Set("value", pref->GetValue()->DeepCopy());
        dict->SetString("controlledBy", "shared");
        return dict;
      }
    }
  }

  return CoreOptionsHandler::CreateValueForPref(pref_name,
                                                controlling_pref_name);
}

void CoreChromeOSOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  CoreOptionsHandler::GetLocalizedValues(localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui());
  AddAccountUITweaksLocalizedValues(localized_strings, profile);

  UserManager* user_manager = UserManager::Get();

  // Check at load time whether this is a secondary user in a multi-profile
  // session.
  User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && user->email() != user_manager->GetPrimaryUser()->email()) {
    const std::string& primary_email = user_manager->GetPrimaryUser()->email();

    // Set secondaryUser to show the shared icon by the network section header.
    localized_strings->SetBoolean("secondaryUser", true);
    localized_strings->SetString("secondaryUserBannerText",
        l10n_util::GetStringFUTF16(
            IDS_OPTIONS_SETTINGS_SECONDARY_USER_BANNER,
            base::ASCIIToUTF16(primary_email)));
    localized_strings->SetString("controlledSettingShared",
        l10n_util::GetStringFUTF16(
            IDS_OPTIONS_CONTROLLED_SETTING_SHARED,
            base::ASCIIToUTF16(primary_email)));
    localized_strings->SetString("controlledSettingsShared",
        l10n_util::GetStringFUTF16(
            IDS_OPTIONS_CONTROLLED_SETTINGS_SHARED,
            base::ASCIIToUTF16(primary_email)));
  } else {
    localized_strings->SetBoolean("secondaryUser", false);
    localized_strings->SetString("secondaryUserBannerText", base::string16());
    localized_strings->SetString("controlledSettingShared", base::string16());
    localized_strings->SetString("controlledSettingsShared", base::string16());
  }

  // Screen lock icon can show up as primary or secondary user.
  localized_strings->SetString("screenLockShared",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_SHARED_SCREEN_LOCK));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    // Managed machines have no "owner".
    localized_strings->SetString("controlledSettingOwner", base::string16());
  } else {
    localized_strings->SetString("controlledSettingOwner",
        l10n_util::GetStringFUTF16(
            IDS_OPTIONS_CONTROLLED_SETTING_OWNER,
            base::ASCIIToUTF16(user_manager->GetOwnerEmail())));
  }
}

void CoreChromeOSOptionsHandler::SelectNetworkCallback(
    const base::ListValue* args) {
  std::string service_path;
  if (args->GetSize() != 1 ||
      !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  proxy_config_service_.SetCurrentNetwork(service_path);
  NotifyProxyPrefsChanged();
}

void CoreChromeOSOptionsHandler::OnPreferenceChanged(
    PrefService* service,
    const std::string& pref_name) {
  // Redetermine the current proxy settings and notify the UI if any of these
  // preferences change.
  if (pref_name == prefs::kOpenNetworkConfiguration ||
      pref_name == prefs::kDeviceOpenNetworkConfiguration ||
      pref_name == prefs::kProxy) {
    NotifyProxyPrefsChanged();
    return;
  }
  if (pref_name == prefs::kUseSharedProxies) {
    // kProxy controls kUseSharedProxies and decides if it's managed by
    // policy/extension.
    NotifyPrefChanged(prefs::kUseSharedProxies, prefs::kProxy);
    return;
  }
  ::options::CoreOptionsHandler::OnPreferenceChanged(service, pref_name);
}

void CoreChromeOSOptionsHandler::NotifySettingsChanged(
    const std::string& setting_name) {
  DCHECK(CrosSettings::Get()->IsCrosSettings(setting_name));
  scoped_ptr<base::Value> value(FetchPref(setting_name));
  if (!value.get())
    NOTREACHED();
  DispatchPrefChangeNotification(setting_name, value.Pass());
}

void CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged() {
  proxy_config_service_.UpdateFromPrefs();
  for (size_t i = 0; i < kProxySettingsCount; ++i) {
    base::Value* value = NULL;
    proxy_cros_settings_parser::GetProxyPrefValue(
        proxy_config_service_, kProxySettings[i], &value);
    DCHECK(value);
    scoped_ptr<base::Value> ptr(value);
    DispatchPrefChangeNotification(kProxySettings[i], ptr.Pass());
  }
}

}  // namespace options
}  // namespace chromeos
