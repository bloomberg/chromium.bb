// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/core_options_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_types.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

CoreOptionsHandler::CoreOptionsHandler()
    : handlers_host_(NULL) {
}

CoreOptionsHandler::~CoreOptionsHandler() {}

void CoreOptionsHandler::Initialize() {
  clear_plugin_lso_data_enabled_.Init(prefs::kClearPluginLSODataEnabled,
                                      Profile::FromWebUI(web_ui_),
                                      this);
  UpdateClearPluginLSOData();
}

void CoreOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings);
}

void CoreOptionsHandler::GetStaticLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Main
  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));

  // Managed prefs
  localized_strings->SetString("policyManagedPrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POLICY_MANAGED_PREFS));
  localized_strings->SetString("extensionManagedPrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_EXTENSION_MANAGED_PREFS));
  localized_strings->SetString("policyAndExtensionManagedPrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POLICY_EXTENSION_MANAGED_PREFS));

  // Controlled settings bubble.
  localized_strings->SetString("controlledSettingPolicy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_POLICY));
  localized_strings->SetString("controlledSettingExtension",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_EXTENSION));
  localized_strings->SetString("controlledSettingRecommended",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_RECOMMENDED));
  localized_strings->SetString("controlledSettingApplyRecommendation",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_APPLY_RECOMMENDATION));

  // Search
  RegisterTitle(localized_strings, "searchPage", IDS_OPTIONS_SEARCH_PAGE_TITLE);
  localized_strings->SetString("searchPlaceholder",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SEARCH_PLACEHOLDER));
  localized_strings->SetString("searchPageNoMatches",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SEARCH_PAGE_NO_MATCHES));
  localized_strings->SetString("searchPageHelpLabel",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SEARCH_PAGE_HELP_LABEL));
  localized_strings->SetString("searchPageHelpTitle",
      l10n_util::GetStringFUTF16(IDS_OPTIONS_SEARCH_PAGE_HELP_TITLE,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
  localized_strings->SetString("searchPageHelpURL",
      google_util::AppendGoogleLocaleParam(
          GURL(chrome::kChromeHelpURL)).spec());

  // Common
  localized_strings->SetString("ok",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString("close",
      l10n_util::GetStringUTF16(IDS_CLOSE));
}

void CoreOptionsHandler::Uninitialize() {
  std::string last_pref;
  for (PreferenceCallbackMap::const_iterator iter = pref_callback_map_.begin();
       iter != pref_callback_map_.end();
       ++iter) {
    if (last_pref != iter->first) {
      StopObservingPref(iter->first);
      last_pref = iter->first;
    }
  }
}

WebUIMessageHandler* CoreOptionsHandler::Attach(WebUI* web_ui) {
  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);
  DCHECK(web_ui_);
  registrar_.Init(Profile::FromWebUI(web_ui_)->GetPrefs());
  return result;
}

void CoreOptionsHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = content::Details<std::string>(details).ptr();
    if (*pref_name == prefs::kClearPluginLSODataEnabled) {
      // This preference is stored in Local State, not in the user preferences.
      UpdateClearPluginLSOData();
      return;
    }
    NotifyPrefChanged(*pref_name, std::string());
  }
}

void CoreOptionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("coreOptionsInitialize",
      base::Bind(&CoreOptionsHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("fetchPrefs",
      base::Bind(&CoreOptionsHandler::HandleFetchPrefs,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("observePrefs",
      base::Bind(&CoreOptionsHandler::HandleObservePrefs,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setBooleanPref",
      base::Bind(&CoreOptionsHandler::HandleSetBooleanPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setIntegerPref",
      base::Bind(&CoreOptionsHandler::HandleSetIntegerPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setDoublePref",
      base::Bind(&CoreOptionsHandler::HandleSetDoublePref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setStringPref",
      base::Bind(&CoreOptionsHandler::HandleSetStringPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setURLPref",
      base::Bind(&CoreOptionsHandler::HandleSetURLPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("setListPref",
      base::Bind(&CoreOptionsHandler::HandleSetListPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("clearPref",
      base::Bind(&CoreOptionsHandler::HandleClearPref,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("coreOptionsUserMetricsAction",
      base::Bind(&CoreOptionsHandler::HandleUserMetricsAction,
                 base::Unretained(this)));
}

void CoreOptionsHandler::HandleInitialize(const ListValue* args) {
  DCHECK(handlers_host_);
  handlers_host_->InitializeHandlers();
}

base::Value* CoreOptionsHandler::FetchPref(const std::string& pref_name) {
  PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref)
    return base::Value::CreateNullValue();

  return CreateValueForPref(pref, NULL);
}

void CoreOptionsHandler::ObservePref(const std::string& pref_name) {
  registrar_.Add(pref_name.c_str(), this);
}

void CoreOptionsHandler::SetPref(const std::string& pref_name,
                                 const base::Value* value,
                                 const std::string& metric) {
  PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();

  switch (value->GetType()) {
    case base::Value::TYPE_BOOLEAN:
    case base::Value::TYPE_INTEGER:
    case base::Value::TYPE_DOUBLE:
    case base::Value::TYPE_STRING:
      pref_service->Set(pref_name.c_str(), *value);
      break;

    default:
      NOTREACHED();
      return;
  }

  pref_service->ScheduleSavePersistentPrefs();

  ProcessUserMetric(value, metric);
}

void CoreOptionsHandler::ClearPref(const std::string& pref_name,
                                   const std::string& metric) {
  PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();
  pref_service->ClearPref(pref_name.c_str());
  pref_service->ScheduleSavePersistentPrefs();

  if (!metric.empty())
    UserMetrics::RecordComputedAction(metric);
}

void CoreOptionsHandler::ProcessUserMetric(const base::Value* value,
                                           const std::string& metric) {
  if (metric.empty())
    return;

  std::string metric_string = metric;
  if (value->IsType(base::Value::TYPE_BOOLEAN)) {
    bool bool_value;
    CHECK(value->GetAsBoolean(&bool_value));
    metric_string += bool_value ? "_Enable" : "_Disable";
  }

  UserMetrics::RecordComputedAction(metric_string);
}

void CoreOptionsHandler::NotifyPrefChanged(
    const std::string& pref_name,
    const std::string& controlling_pref_name) {
  const PrefService* pref_service = Profile::FromWebUI(web_ui_)->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());
  if (!pref)
    return;
  const PrefService::Preference* controlling_pref =
      !controlling_pref_name.empty() ?
          pref_service->FindPreference(controlling_pref_name.c_str()) : NULL;
  std::pair<PreferenceCallbackMap::const_iterator,
            PreferenceCallbackMap::const_iterator> range;
  range = pref_callback_map_.equal_range(pref_name);
  for (PreferenceCallbackMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    const std::wstring& callback_function = iter->second;
    ListValue result_value;
    result_value.Append(base::Value::CreateStringValue(pref_name.c_str()));
    result_value.Append(CreateValueForPref(pref, controlling_pref));
    web_ui_->CallJavascriptFunction(WideToASCII(callback_function),
                                    result_value);
  }
}

DictionaryValue* CoreOptionsHandler::CreateValueForPref(
    const PrefService::Preference* pref,
    const PrefService::Preference* controlling_pref) {
  DictionaryValue* dict = new DictionaryValue;
  dict->Set("value", pref->GetValue()->DeepCopy());
  if (!controlling_pref)  // No controlling pref is managing actual pref.
    controlling_pref = pref;  // This means pref is controlling itself.
  if (controlling_pref->IsManaged()) {
    dict->SetString("controlledBy", "policy");
  } else if (controlling_pref->IsExtensionControlled()) {
    dict->SetString("controlledBy", "extension");
  } else if (controlling_pref->IsUserModifiable() &&
             !controlling_pref->IsDefaultValue()) {
    dict->SetString("controlledBy", "recommended");
  }
  dict->SetBoolean("disabled", !controlling_pref->IsUserModifiable());
  return dict;
}

void CoreOptionsHandler::StopObservingPref(const std::string& path) {
  registrar_.Remove(path.c_str(), this);
}

void CoreOptionsHandler::HandleFetchPrefs(const ListValue* args) {
  // First param is name of callback function, so, there needs to be at least
  // one more element for the actual preference identifier.
  DCHECK_GE(static_cast<int>(args->GetSize()), 2);

  // Get callback JS function name.
  base::Value* callback;
  if (!args->Get(0, &callback) || !callback->IsType(base::Value::TYPE_STRING))
    return;

  string16 callback_function;
  if (!callback->GetAsString(&callback_function))
    return;

  // Get the list of name for prefs to build the response dictionary.
  DictionaryValue result_value;
  base::Value* list_member;

  for (size_t i = 1; i < args->GetSize(); i++) {
    if (!args->Get(i, &list_member))
      break;

    if (!list_member->IsType(base::Value::TYPE_STRING))
      continue;

    std::string pref_name;
    if (!list_member->GetAsString(&pref_name))
      continue;

    result_value.Set(pref_name.c_str(), FetchPref(pref_name));
  }
  web_ui_->CallJavascriptFunction(UTF16ToASCII(callback_function),
                                  result_value);
}

void CoreOptionsHandler::HandleObservePrefs(const ListValue* args) {
  // First param is name is JS callback function name, the rest are pref
  // identifiers that we are observing.
  DCHECK_GE(static_cast<int>(args->GetSize()), 2);

  // Get preference change callback function name.
  string16 callback_func_name;
  if (!args->GetString(0, &callback_func_name))
    return;

  // Get all other parameters - pref identifiers.
  for (size_t i = 1; i < args->GetSize(); i++) {
    base::Value* list_member;
    if (!args->Get(i, &list_member))
      break;

    // Just ignore bad pref identifiers for now.
    std::string pref_name;
    if (!list_member->IsType(base::Value::TYPE_STRING) ||
        !list_member->GetAsString(&pref_name))
      continue;

    if (pref_callback_map_.find(pref_name) == pref_callback_map_.end())
      ObservePref(pref_name);

    pref_callback_map_.insert(
        PreferenceCallbackMap::value_type(pref_name,
                                          UTF16ToWideHack(callback_func_name)));
  }
}

void CoreOptionsHandler::HandleSetBooleanPref(const ListValue* args) {
  HandleSetPref(args, TYPE_BOOLEAN);
}

void CoreOptionsHandler::HandleSetIntegerPref(const ListValue* args) {
  HandleSetPref(args, TYPE_INTEGER);
}

void CoreOptionsHandler::HandleSetDoublePref(const ListValue* args) {
  HandleSetPref(args, TYPE_DOUBLE);
}

void CoreOptionsHandler::HandleSetStringPref(const ListValue* args) {
  HandleSetPref(args, TYPE_STRING);
}

void CoreOptionsHandler::HandleSetURLPref(const ListValue* args) {
  HandleSetPref(args, TYPE_URL);
}

void CoreOptionsHandler::HandleSetListPref(const ListValue* args) {
  HandleSetPref(args, TYPE_LIST);
}

void CoreOptionsHandler::HandleSetPref(const ListValue* args, PrefType type) {
  DCHECK_GT(static_cast<int>(args->GetSize()), 1);

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  base::Value* value;
  if (!args->Get(1, &value))
    return;

  scoped_ptr<base::Value> temp_value;

  switch (type) {
    case TYPE_BOOLEAN:
      CHECK_EQ(base::Value::TYPE_BOOLEAN, value->GetType());
      break;
    case TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      CHECK(value->GetAsDouble(&double_value));
      int int_value = static_cast<int>(double_value);
      temp_value.reset(base::Value::CreateIntegerValue(int_value));
      value = temp_value.get();
      break;
    }
    case TYPE_DOUBLE:
      CHECK_EQ(base::Value::TYPE_DOUBLE, value->GetType());
      break;
    case TYPE_STRING:
      CHECK_EQ(base::Value::TYPE_STRING, value->GetType());
      break;
    case TYPE_URL: {
      std::string original;
      CHECK(value->GetAsString(&original));
      GURL fixed = URLFixerUpper::FixupURL(original, std::string());
      temp_value.reset(base::Value::CreateStringValue(fixed.spec()));
      value = temp_value.get();
      break;
    }
    case TYPE_LIST: {
      // In case we have a List pref we got a JSON string.
      std::string json_string;
      CHECK(value->GetAsString(&json_string));
      temp_value.reset(
          base::JSONReader().JsonToValue(json_string,
                                         false,  // no check_root
                                         false));  // no trailing comma
      value = temp_value.get();
      CHECK_EQ(base::Value::TYPE_LIST, value->GetType());
      break;
    }
    default:
      NOTREACHED();
  }

  std::string metric;
  if (args->GetSize() > 2 && !args->GetString(2, &metric))
    LOG(WARNING) << "Invalid metric parameter: " << pref_name;
  SetPref(pref_name, value, metric);
}

void CoreOptionsHandler::HandleClearPref(const ListValue* args) {
  DCHECK_GT(static_cast<int>(args->GetSize()), 0);

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  std::string metric;
  if (args->GetSize() > 1)
    args->GetString(1, &metric);

  ClearPref(pref_name, metric);
}

void CoreOptionsHandler::HandleUserMetricsAction(const ListValue* args) {
  std::string metric = UTF16ToUTF8(ExtractStringValue(args));
  if (!metric.empty())
    UserMetrics::RecordComputedAction(metric);
}

void CoreOptionsHandler::UpdateClearPluginLSOData() {
  scoped_ptr<base::Value> enabled(
      base::Value::CreateBooleanValue(
          clear_plugin_lso_data_enabled_.GetValue()));
  web_ui_->CallJavascriptFunction(
      "OptionsPage.setClearPluginLSODataEnabled", *enabled);
}
