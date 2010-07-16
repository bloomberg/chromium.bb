// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/core_options_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

CoreOptionsHandler::CoreOptionsHandler() {
}

CoreOptionsHandler::~CoreOptionsHandler() {
  // Remove registered preference change notification observers.
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  std::wstring last_pref;
  for (PreferenceCallbackMap::const_iterator iter = pref_callback_map_.begin();
       iter != pref_callback_map_.end();
       ++iter) {
    if (last_pref != iter->first) {
      pref_service->RemovePrefObserver(iter->first.c_str(), this);
      last_pref = iter->first;
    }
  }
}

void CoreOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Main
  localized_strings->SetString(L"title",
      l10n_util::GetStringF(IDS_OPTIONS_DIALOG_TITLE,
          l10n_util::GetString(IDS_PRODUCT_NAME)));

  localized_strings->SetString(L"browserPage",
      l10n_util::GetString(IDS_OPTIONS_GENERAL_TAB_LABEL));
  localized_strings->SetString(L"personalPage",
      l10n_util::GetString(IDS_OPTIONS_CONTENT_TAB_LABEL));
  localized_strings->SetString(L"advancedPage",
      l10n_util::GetString(IDS_OPTIONS_ADVANCED_TAB_LABEL));
#if defined(OS_CHROMEOS)
  localized_strings->SetString(L"internetPage",
      l10n_util::GetString(IDS_OPTIONS_INTERNET_TAB_LABEL));
  localized_strings->SetString(L"languageHangulPage",
      l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_TITLE));
#endif
}

void CoreOptionsHandler::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

void CoreOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("coreOptionsInitialize",
      NewCallback(this, &CoreOptionsHandler::HandleInitialize));
  dom_ui_->RegisterMessageCallback("fetchPrefs",
      NewCallback(this, &CoreOptionsHandler::HandleFetchPrefs));
  dom_ui_->RegisterMessageCallback("observePrefs",
      NewCallback(this, &CoreOptionsHandler::HandleObservePrefs));
  dom_ui_->RegisterMessageCallback("setBooleanPref",
      NewCallback(this, &CoreOptionsHandler::HandleSetBooleanPref));
  dom_ui_->RegisterMessageCallback("setIntegerPref",
      NewCallback(this, &CoreOptionsHandler::HandleSetIntegerPref));
  dom_ui_->RegisterMessageCallback("setStringPref",
      NewCallback(this, &CoreOptionsHandler::HandleSetStringPref));
  dom_ui_->RegisterMessageCallback("setObjectPref",
      NewCallback(this, &CoreOptionsHandler::HandleSetObjectPref));
}

void CoreOptionsHandler::HandleInitialize(const Value* value) {
  (static_cast<OptionsUI*>(dom_ui_))->InitializeHandlers();
}

Value* CoreOptionsHandler::FetchPref(const std::wstring& pref_name) {
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();

  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name.c_str());

  return pref ? pref->GetValue()->DeepCopy() : Value::CreateNullValue();
}

void CoreOptionsHandler::ObservePref(const std::wstring& pref_name) {
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();

  pref_service->AddPrefObserver(pref_name.c_str(), this);
}

void CoreOptionsHandler::SetPref(const std::wstring& pref_name,
                                 Value::ValueType pref_type,
                                 const std::string& value_string) {
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();

  switch (pref_type) {
    case Value::TYPE_BOOLEAN:
      pref_service->SetBoolean(pref_name.c_str(), value_string == "true");
      break;
    case Value::TYPE_INTEGER:
      int int_value;
      if (StringToInt(value_string, &int_value))
        pref_service->SetInteger(pref_name.c_str(), int_value);
      break;
    case Value::TYPE_STRING:
      pref_service->SetString(pref_name.c_str(), value_string);
      break;
    default:
      NOTREACHED();
  }
}

void CoreOptionsHandler::HandleFetchPrefs(const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST))
    return;

  const ListValue* param_values = static_cast<const ListValue*>(value);

  // First param is name of callback function, so, there needs to be at least
  // one more element for the actual preference identifier.
  const size_t kMinFetchPrefsParamCount = 2;
  if (param_values->GetSize() < kMinFetchPrefsParamCount)
    return;

  size_t idx = param_values->GetSize();
  LOG(INFO) << "param_values->GetSize() = " << idx;
  // Get callback JS function name.
  Value* callback;
  if (!param_values->Get(0, &callback) || !callback->IsType(Value::TYPE_STRING))
    return;

  std::wstring callback_function;
  if (!callback->GetAsString(&callback_function))
    return;

  // Get the list of name for prefs to build the response dictionary.
  DictionaryValue result_value;
  Value* list_member;

  for (size_t i = 1; i < param_values->GetSize(); i++) {
    if (!param_values->Get(i, &list_member))
      break;

    if (!list_member->IsType(Value::TYPE_STRING))
      continue;

    std::wstring pref_name;
    if (!list_member->GetAsString(&pref_name))
      continue;

    result_value.Set(pref_name.c_str(), FetchPref(pref_name));
  }
  dom_ui_->CallJavascriptFunction(callback_function.c_str(), result_value);
}

void CoreOptionsHandler::HandleObservePrefs(const Value* value) {
  if (!value || !value->IsType(Value::TYPE_LIST))
    return;

  DictionaryValue result_value;
  const ListValue* list_value = static_cast<const ListValue*>(value);

  // First param is name is JS callback function name, the rest are pref
  // identifiers that we are observing.
  const size_t kMinObservePrefsParamCount = 2;
  if (list_value->GetSize() < kMinObservePrefsParamCount)
    return;

  // Get preference change callback function name.
  std::wstring callback_func_name;
  if (!list_value->GetString(0, &callback_func_name))
    return;

  // Get all other parameters - pref identifiers.
  for (size_t i = 1; i < list_value->GetSize(); i++) {
    Value* list_member;
    if (!list_value->Get(i, &list_member))
      break;

    // Just ignore bad pref identifiers for now.
    std::wstring pref_name;
    if (!list_member->IsType(Value::TYPE_STRING) ||
        !list_member->GetAsString(&pref_name))
      continue;

    if (pref_callback_map_.find(pref_name) == pref_callback_map_.end())
      ObservePref(pref_name);

    pref_callback_map_.insert(
        PreferenceCallbackMap::value_type(pref_name, callback_func_name));
  }
}

void CoreOptionsHandler::HandleSetBooleanPref(const Value* value) {
  HandleSetPref(value, Value::TYPE_BOOLEAN);
}

void CoreOptionsHandler::HandleSetIntegerPref(const Value* value) {
  HandleSetPref(value, Value::TYPE_INTEGER);
}

void CoreOptionsHandler::HandleSetStringPref(const Value* value) {
  HandleSetPref(value, Value::TYPE_STRING);
}

void CoreOptionsHandler::HandleSetObjectPref(const Value* value) {
  HandleSetPref(value, Value::TYPE_NULL);
}

void CoreOptionsHandler::HandleSetPref(const Value* value,
                                       Value::ValueType type) {
  if (!value || !value->IsType(Value::TYPE_LIST))
    return;
  const ListValue* param_values = static_cast<const ListValue*>(value);
  size_t size = param_values->GetSize();
  LOG(INFO) << "Array size = " << size;
  if (param_values->GetSize() != 2)
    return;

  std::wstring pref_name;
  if (!param_values->GetString(0, &pref_name))
    return;

  std::string value_string;
  if (!param_values->GetString(1, &value_string))
    return;

  SetPref(pref_name, type, value_string);
}

void CoreOptionsHandler::NotifyPrefChanged(const std::wstring* pref_name) {
  DCHECK(pref_name);
  DCHECK(dom_ui_);
  PrefService* pref_service = dom_ui_->GetProfile()->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name->c_str());
  if (pref) {
    for (PreferenceCallbackMap::const_iterator iter =
        pref_callback_map_.find(*pref_name);
        iter != pref_callback_map_.end(); ++iter) {
      const std::wstring& callback_function = iter->second;
      ListValue result_value;
      result_value.Append(Value::CreateStringValue(pref_name->c_str()));
      result_value.Append(pref->GetValue()->DeepCopy());
      dom_ui_->CallJavascriptFunction(callback_function, result_value);
    }
  }
}
