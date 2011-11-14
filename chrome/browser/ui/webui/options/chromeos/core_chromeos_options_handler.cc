// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

#include <string>

#include "base/bind.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/proxy_cros_settings_parser.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace chromeos {

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

  base::Value* pref_value = NULL;
  CrosSettings::Get()->Get(pref_name, &pref_value);
  return pref_value ? pref_value : base::Value::CreateNullValue();
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (proxy_cros_settings_parser::IsProxyPref(pref_name)) {
    // We observe those all the time.
    return;
  }
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::ObservePref(pref_name);
  // TODO(xiyuan): Change this when CrosSettings supports observers.
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
  // CrosSettings takes ownership of its value so we need to copy it.
  base::Value* pref_value = value->DeepCopy();
  CrosSettings::Get()->Set(pref_name, pref_value);
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
  base::Value* value = NULL;
  if (!CrosSettings::Get()->Get(*setting_name, &value)) {
    NOTREACHED();
    if (value)
      delete value;
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
