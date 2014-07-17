// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private_api.h"

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/metrics/metrics_service.h"
#include "extensions/common/error_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;

namespace extensions {

namespace {

// Key which corresponds to the HWID setting.
const char kPropertyHWID[] = "hwid";

// Key which corresponds to the customization ID setting.
const char kPropertyCustomizationID[] = "customizationId";

// Key which corresponds to the home provider property.
const char kPropertyHomeProvider[] = "homeProvider";

// Key which corresponds to the initial_locale property.
const char kPropertyInitialLocale[] = "initialLocale";

// Key which corresponds to the board property in JS.
const char kPropertyBoard[] = "board";

// Key which corresponds to the isOwner property in JS.
const char kPropertyOwner[] = "isOwner";

// Key which corresponds to the clientId property in JS.
const char kPropertyClientId[] = "clientId";

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Key which corresponds to the timezone property in JS.
const char kPropertySupportedTimezones[] = "supportedTimezones";

// Key which corresponds to the large cursor A11Y property in JS.
const char kPropertyLargeCursorEnabled[] = "a11yLargeCursorEnabled";

// Key which corresponds to the sticky keys A11Y property in JS.
const char kPropertyStickyKeysEnabled[] = "a11yStickyKeysEnabled";

// Key which corresponds to the spoken feedback A11Y property in JS.
const char kPropertySpokenFeedbackEnabled[] = "a11ySpokenFeedbackEnabled";

// Key which corresponds to the high contrast mode A11Y property in JS.
const char kPropertyHighContrastEnabled[] = "a11yHighContrastEnabled";

// Key which corresponds to the screen magnifier A11Y property in JS.
const char kPropertyScreenMagnifierEnabled[] = "a11yScreenMagnifierEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyAutoclickEnabled[] = "a11yAutoClickEnabled";

// Key which corresponds to the auto click A11Y property in JS.
const char kPropertyVirtualKeyboardEnabled[] = "a11yVirtualKeyboardEnabled";

// Key which corresponds to the send-function-keys property in JS.
const char kPropertySendFunctionsKeys[] = "sendFunctionKeys";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";

const struct {
  const char* api_name;
  const char* preference_name;
} kPreferencesMap[] = {
      {kPropertyLargeCursorEnabled, prefs::kAccessibilityLargeCursorEnabled},
      {kPropertyStickyKeysEnabled, prefs::kAccessibilityStickyKeysEnabled},
      {kPropertySpokenFeedbackEnabled,
       prefs::kAccessibilitySpokenFeedbackEnabled},
      {kPropertyHighContrastEnabled, prefs::kAccessibilityHighContrastEnabled},
      {kPropertyScreenMagnifierEnabled,
       prefs::kAccessibilityScreenMagnifierEnabled},
      {kPropertyAutoclickEnabled, prefs::kAccessibilityAutoclickEnabled},
      {kPropertyVirtualKeyboardEnabled,
       prefs::kAccessibilityVirtualKeyboardEnabled},
      {kPropertySendFunctionsKeys, prefs::kLanguageSendFunctionKeys}};

const char* GetBoolPrefNameForApiProperty(const char* api_name) {
  for (size_t i = 0;
       i < (sizeof(kPreferencesMap)/sizeof(*kPreferencesMap));
       i++) {
    if (strcmp(kPreferencesMap[i].api_name, api_name) == 0)
      return kPreferencesMap[i].preference_name;
  }

  return NULL;
}

bool IsEnterpriseKiosk() {
  if (!chrome::IsRunningInForcedAppMode())
    return false;

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}

std::string GetClientId() {
  return IsEnterpriseKiosk()
             ? g_browser_process->metrics_service()->GetClientId()
             : std::string();
}

}  // namespace

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() {
}

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() {
}

bool ChromeosInfoPrivateGetFunction::RunAsync() {
  base::ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(0, &list));
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string property_name;
    EXTENSION_FUNCTION_VALIDATE(list->GetString(i, &property_name));
    base::Value* value = GetValue(property_name);
    if (value)
      result->Set(property_name, value);
  }
  SetResult(result.release());
  SendResponse(true);
  return true;
}

base::Value* ChromeosInfoPrivateGetFunction::GetValue(
    const std::string& property_name) {
  if (property_name == kPropertyHWID) {
    std::string hwid;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kHardwareClassKey, &hwid);
    return new base::StringValue(hwid);
  } else if (property_name == kPropertyCustomizationID) {
    std::string customization_id;
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();
    provider->GetMachineStatistic(chromeos::system::kCustomizationIdKey,
                                  &customization_id);
    return new base::StringValue(customization_id);
  } else if (property_name == kPropertyHomeProvider) {
    const chromeos::DeviceState* cellular_device =
        NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
            chromeos::NetworkTypePattern::Cellular());
    std::string home_provider_id;
    if (cellular_device)
      home_provider_id = cellular_device->home_provider_id();
    return new base::StringValue(home_provider_id);
  } else if (property_name == kPropertyInitialLocale) {
    return new base::StringValue(
        chromeos::StartupUtils::GetInitialLocale());
  } else if (property_name == kPropertyBoard) {
    return new base::StringValue(base::SysInfo::GetLsbReleaseBoard());
  } else if (property_name == kPropertyOwner) {
    return new base::FundamentalValue(
        chromeos::UserManager::Get()->IsCurrentUserOwner());
  } else if (property_name == kPropertyClientId) {
    return new base::StringValue(GetClientId());
  } else if (property_name == kPropertyTimezone) {
    return chromeos::CrosSettings::Get()->GetPref(
            chromeos::kSystemTimezone)->DeepCopy();
  } else if (property_name == kPropertySupportedTimezones) {
    scoped_ptr<base::ListValue> values = chromeos::system::GetTimezoneList();
    return values.release();
  } else {
    const char* pref_name =
        GetBoolPrefNameForApiProperty(property_name.c_str());
    if (pref_name) {
      return new base::FundamentalValue(
          Profile::FromBrowserContext(context_)->GetPrefs()->GetBoolean(
              pref_name));
    }
  }

  DLOG(ERROR) << "Unknown property request: " << property_name;
  return NULL;
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() {
}

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() {
}

bool ChromeosInfoPrivateSetFunction::RunSync() {
  std::string param_name;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &param_name));
  if (param_name == kPropertyTimezone) {
    std::string param_value;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &param_value));
    chromeos::CrosSettings::Get()->Set(chromeos::kSystemTimezone,
                                       base::StringValue(param_value));
  } else {
    const char* pref_name = GetBoolPrefNameForApiProperty(param_name.c_str());
    if (pref_name) {
      bool param_value;
      EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &param_value));
      Profile::FromBrowserContext(context_)->GetPrefs()->SetBoolean(
          pref_name,
          param_value);
    } else {
      error_ = ErrorUtils::FormatErrorMessage(kPropertyNotFound, param_name);
      return false;
    }
  }

  return true;
}

}  // namespace extensions
