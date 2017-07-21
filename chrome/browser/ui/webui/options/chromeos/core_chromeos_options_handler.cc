// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#include "chrome/browser/ui/webui/options/chromeos/accounts_options_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

namespace {

// List of settings that should be changeable by all users.
const char* kNonPrivilegedSettings[] = {
    kSystemTimezone
};

// List of settings that should only be changeable by the primary user.
const char* kPrimaryUserSettings[] = {
    prefs::kWakeOnWifiDarkConnect,
};

// Returns true if |pref| can be controlled (e.g. by policy or owner).
bool IsSettingPrivileged(const std::string& pref) {
  const char** end = kNonPrivilegedSettings + arraysize(kNonPrivilegedSettings);
  return std::find(kNonPrivilegedSettings, end, pref) == end;
}

// Returns true if |pref| is shared (controlled by the primary user).
bool IsSettingShared(const std::string& pref) {
  const char** end = kPrimaryUserSettings + arraysize(kPrimaryUserSettings);
  return std::find(kPrimaryUserSettings, end, pref) != end;
}

// Creates a user info dictionary to be stored in the |ListValue| that is
// passed to Javascript for the |kAccountsPrefUsers| preference.
std::unique_ptr<base::DictionaryValue> CreateUserInfo(
    const std::string& username,
    const std::string& display_email,
    const std::string& display_name) {
  auto user_dict = base::MakeUnique<base::DictionaryValue>();
  user_dict->SetString("username", username);
  user_dict->SetString("name", display_email);
  user_dict->SetString("email", display_name);

  const bool is_owner =
      user_manager::UserManager::Get()->GetOwnerAccountId().GetUserEmail() ==
      username;
  user_dict->SetBoolean("owner", is_owner);
  return user_dict;
}

// This function decorates the bare list of emails with some more information
// needed by the UI to properly display the Accounts page.
std::unique_ptr<base::Value> CreateUsersWhitelist(
    const base::Value* pref_value) {
  const base::ListValue* list_value =
      static_cast<const base::ListValue*>(pref_value);
  auto user_list = base::MakeUnique<base::ListValue>();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  for (base::ListValue::const_iterator i = list_value->begin();
       i != list_value->end(); ++i) {
    std::string email;
    if (i->GetAsString(&email)) {
      // Translate email to the display email.
      const std::string display_email =
          user_manager->GetUserDisplayEmail(AccountId::FromUserEmail(email));
      // TODO(ivankr): fetch display name for existing users.
      user_list->Append(CreateUserInfo(email, display_email, std::string()));
    }
  }
  return std::move(user_list);
}

// Checks whether this is a secondary user in a multi-profile session.
bool IsSecondaryUser(Profile* profile) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  return user &&
         user->GetAccountId() != user_manager->GetPrimaryUser()->GetAccountId();
}

const char kSelectNetworkMessage[] = "selectNetwork";

UIProxyConfigService* GetUiProxyConfigService() {
  return NetworkHandler::Get()->ui_proxy_config_service();
}

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
  CoreOptionsHandler::InitializeHandler();

  if (!ProfileHelper::IsSigninProfile(Profile::FromWebUI(web_ui())))
    ObservePref(onc::prefs::kOpenNetworkConfiguration);
  ObservePref(proxy_config::prefs::kProxy);
  ObservePref(onc::prefs::kDeviceOpenNetworkConfiguration);
}

void CoreChromeOSOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_OWNERSHIP_STATUS_CHANGED, type);

  // Finish this asynchronously because the notification has to tricle in to all
  // Chrome components before we can reliably read the status on the other end.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&CoreChromeOSOptionsHandler::NotifyOwnershipChanged,
                            base::Unretained(this)));
}

void CoreChromeOSOptionsHandler::NotifyOwnershipChanged() {
  for (auto& it : pref_subscription_map_)
    NotifySettingsChanged(it.first);
}

std::unique_ptr<base::Value> CoreChromeOSOptionsHandler::FetchPref(
    const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    std::unique_ptr<base::Value> value;
    proxy_cros_settings_parser::GetProxyPrefValue(
        network_guid_, pref_name, GetUiProxyConfigService(), &value);
    return value;
  }

  Profile* profile = Profile::FromWebUI(web_ui());
  if (!CrosSettings::IsCrosSettings(pref_name)) {
    std::string controlling_pref =
        pref_name == proxy_config::prefs::kUseSharedProxies
            ? proxy_config::prefs::kProxy
            : std::string();
    std::unique_ptr<base::Value> value =
        CreateValueForPref(pref_name, controlling_pref);
    if (!IsSettingShared(pref_name) || !IsSecondaryUser(profile))
      return value;
    base::DictionaryValue* dict;
    if (!value->GetAsDictionary(&dict) || dict->HasKey("controlledBy"))
      return value;
    Profile* primary_profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetPrimaryUser());
    if (!primary_profile)
      return value;
    dict->SetString("controlledBy", "shared");
    dict->SetBoolean("disabled", true);
    dict->SetBoolean("value", primary_profile->GetPrefs()->GetBoolean(
        pref_name));
    return value;
  }

  const base::Value* pref_value = CrosSettings::Get()->GetPref(pref_name);
  if (!pref_value)
    return base::MakeUnique<base::Value>();

  // Decorate pref value as CoreOptionsHandler::CreateValueForPref() does.
  // TODO(estade): seems that this should replicate CreateValueForPref less.
  auto dict = base::MakeUnique<base::DictionaryValue>();
  if (pref_name == kAccountsPrefUsers)
    dict->Set("value", CreateUsersWhitelist(pref_value));
  else
    dict->Set("value", base::MakeUnique<base::Value>(*pref_value));

  std::string controlled_by;
  if (IsSettingPrivileged(pref_name)) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    if (connector->IsEnterpriseManaged())
      controlled_by = "policy";
    else if (!ProfileHelper::IsOwnerProfile(profile))
      controlled_by = "owner";
  }
  dict->SetBoolean("disabled", !controlled_by.empty());
  if (!controlled_by.empty())
    dict->SetString("controlledBy", controlled_by);
  return std::move(dict);
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    // We observe those all the time.
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::options::CoreOptionsHandler::ObservePref(pref_name);

  std::unique_ptr<CrosSettings::ObserverSubscription> subscription =
      CrosSettings::Get()->AddSettingsObserver(
          pref_name.c_str(),
          base::Bind(&CoreChromeOSOptionsHandler::NotifySettingsChanged,
                     base::Unretained(this), pref_name));
  pref_subscription_map_.insert(make_pair(pref_name, std::move(subscription)));
}

void CoreChromeOSOptionsHandler::SetPref(const std::string& pref_name,
                                         const base::Value* value,
                                         const std::string& metric) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    proxy_cros_settings_parser::SetProxyPrefValue(
        network_guid_, pref_name, value, GetUiProxyConfigService());
    base::Value proxy_type(pref_name);
    web_ui()->CallJavascriptFunctionUnsafe(
        "options.internet.DetailsInternetPage.updateProxySettings", proxy_type);
    ProcessUserMetric(value, metric);
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::options::CoreOptionsHandler::SetPref(pref_name, value, metric);
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOS::FromWebUI(web_ui());
  if (service && service->HandlesSetting(pref_name))
    service->Set(pref_name, *value);
  else
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

std::unique_ptr<base::Value> CoreChromeOSOptionsHandler::CreateValueForPref(
    const std::string& pref_name,
    const std::string& controlling_pref_name) {
  // The screen lock setting is shared if multiple users are logged in and at
  // least one has chosen to require passwords.
  if (pref_name == prefs::kEnableAutoScreenLock &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1 &&
      controlling_pref_name.empty()) {
    PrefService* user_prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    const PrefService::Preference* pref =
        user_prefs->FindPreference(prefs::kEnableAutoScreenLock);

    if (pref && pref->IsUserModifiable() &&
        SessionControllerClient::ShouldLockScreenAutomatically()) {
      bool screen_lock = false;
      bool success = pref->GetValue()->GetAsBoolean(&screen_lock);
      DCHECK(success);
      if (!screen_lock) {
        // Screen lock is enabled for the session, but not in the user's
        // preferences. Show the user's value in the checkbox, but indicate
        // that the password requirement is enabled by some other user.
        auto dict = base::MakeUnique<base::DictionaryValue>();
        dict->Set("value", base::MakeUnique<base::Value>(*pref->GetValue()));
        dict->SetString("controlledBy", "shared");
        return std::move(dict);
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

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  if (IsSecondaryUser(profile)) {
    const std::string& primary_email =
        user_manager->GetPrimaryUser()->GetAccountId().GetUserEmail();

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
    localized_strings->SetString(
        "controlledSettingOwner",
        l10n_util::GetStringFUTF16(
            IDS_OPTIONS_CONTROLLED_SETTING_OWNER,
            base::ASCIIToUTF16(
                user_manager->GetOwnerAccountId().GetUserEmail())));
  }
}

void CoreChromeOSOptionsHandler::SelectNetworkCallback(
    const base::ListValue* args) {
  if (args->GetSize() != 1 || !args->GetString(0, &network_guid_)) {
    NOTREACHED();
    return;
  }
  NotifyProxyPrefsChanged();
}

void CoreChromeOSOptionsHandler::OnPreferenceChanged(
    PrefService* service,
    const std::string& pref_name) {
  // Redetermine the current proxy settings and notify the UI if any of these
  // preferences change.
  if (pref_name == onc::prefs::kOpenNetworkConfiguration ||
      pref_name == onc::prefs::kDeviceOpenNetworkConfiguration ||
      pref_name == proxy_config::prefs::kProxy) {
    NotifyProxyPrefsChanged();
    return;
  }
  if (pref_name == proxy_config::prefs::kUseSharedProxies) {
    // kProxy controls kUseSharedProxies and decides if it's managed by
    // policy/extension.
    NotifyPrefChanged(proxy_config::prefs::kUseSharedProxies,
                      proxy_config::prefs::kProxy);
    return;
  }
  ::options::CoreOptionsHandler::OnPreferenceChanged(service, pref_name);
}

void CoreChromeOSOptionsHandler::NotifySettingsChanged(
    const std::string& setting_name) {
  DCHECK(CrosSettings::Get()->IsCrosSettings(setting_name));
  std::unique_ptr<base::Value> value(FetchPref(setting_name));
  if (!value.get())
    NOTREACHED();
  DispatchPrefChangeNotification(setting_name, std::move(value));
}

void CoreChromeOSOptionsHandler::NotifyProxyPrefsChanged() {
  GetUiProxyConfigService()->UpdateFromPrefs(network_guid_);
  for (size_t i = 0; i < proxy_cros_settings_parser::kProxySettingsCount; ++i) {
    std::unique_ptr<base::Value> value;
    proxy_cros_settings_parser::GetProxyPrefValue(
        network_guid_, proxy_cros_settings_parser::kProxySettings[i],
        GetUiProxyConfigService(), &value);
    DCHECK(value);
    DispatchPrefChangeNotification(
        proxy_cros_settings_parser::kProxySettings[i], std::move(value));
  }
}

}  // namespace options
}  // namespace chromeos
