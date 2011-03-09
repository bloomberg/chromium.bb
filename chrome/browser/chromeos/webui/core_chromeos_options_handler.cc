// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/webui/core_chromeos_options_handler.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

namespace chromeos {

CoreChromeOSOptionsHandler::CoreChromeOSOptionsHandler()
    : handling_change_(false) {
}

Value* CoreChromeOSOptionsHandler::FetchPref(const std::string& pref_name) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::FetchPref(pref_name);

  Value* pref_value = NULL;
  CrosSettings::Get()->Get(pref_name, &pref_value);
  return pref_value ? pref_value : Value::CreateNullValue();
}

void CoreChromeOSOptionsHandler::ObservePref(const std::string& pref_name) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::ObservePref(pref_name);

  // TODO(xiyuan): Change this when CrosSettings supports observers.
  CrosSettings::Get()->AddSettingsObserver(pref_name.c_str(), this);
}

void CoreChromeOSOptionsHandler::SetPref(const std::string& pref_name,
                                         const Value* value,
                                         const std::string& metric) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::SetPref(pref_name, value, metric);
  handling_change_ = true;
  // CrosSettings takes ownership of its value so we need to copy it.
  Value* pref_value = value->DeepCopy();
  CrosSettings::Get()->Set(pref_name, pref_value);
  handling_change_ = false;

  ProcessUserMetric(value, metric);
}

void CoreChromeOSOptionsHandler::StopObservingPref(const std::string& path) {
  // Unregister this instance from observing prefs of chrome os settings.
  if (CrosSettings::IsCrosSettings(path))
    CrosSettings::Get()->RemoveSettingsObserver(path.c_str(), this);
  else  // Call base class to handle regular preferences.
    ::CoreOptionsHandler::StopObservingPref(path);
}

void CoreChromeOSOptionsHandler::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  // Ignore the notification if this instance had caused it.
  if (handling_change_)
    return;
  if (type == NotificationType::SYSTEM_SETTING_CHANGED) {
    NotifySettingsChanged(Details<std::string>(details).ptr());
    return;
  }
  ::CoreOptionsHandler::Observe(type, source, details);
}

void CoreChromeOSOptionsHandler::NotifySettingsChanged(
    const std::string* setting_name) {
  DCHECK(web_ui_);
  DCHECK(CrosSettings::Get()->IsCrosSettings(*setting_name));
  Value* value = NULL;
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
    result_value.Append(Value::CreateStringValue(setting_name->c_str()));
    result_value.Append(value->DeepCopy());
    web_ui_->CallJavascriptFunction(WideToASCII(callback_function),
                                    result_value);
  }
  if (value)
    delete value;
}

}  // namespace chromeos
