// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/core_options_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::UserMetricsAction;

namespace options {

namespace {

// Whether "controlledBy" property of pref value sent to options web UI needs to
// be set to "extension" when the preference is controlled by an extension.
bool CanSetExtensionControlledPrefValue(
    const PrefService::Preference* preference) {
#if defined(OS_WIN)
  // These have more obvious UI than the standard one for extension controlled
  // values (an extension puzzle piece) on the settings page. To avoiding
  // showing the extension puzzle piece for these settings, their "controlledBy"
  // value should never be set to "extension".
  return preference->name() != prefs::kURLsToRestoreOnStartup &&
         preference->name() != prefs::kRestoreOnStartup &&
         preference->name() != prefs::kHomePage &&
         preference->name() != prefs::kHomePageIsNewTabPage;
#else
  return true;
#endif
}

}  // namespace

CoreOptionsHandler::CoreOptionsHandler()
    : handlers_host_(NULL) {
}

CoreOptionsHandler::~CoreOptionsHandler() {}

void CoreOptionsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());

  plugin_status_pref_setter_.Init(
      profile,
      base::Bind(&CoreOptionsHandler::OnPreferenceChanged,
                 base::Unretained(this),
                 profile->GetPrefs()));

  pref_change_filters_[prefs::kBrowserGuestModeEnabled] =
      base::Bind(&CoreOptionsHandler::IsUserUnsupervised,
                 base::Unretained(this));
  pref_change_filters_[prefs::kBrowserAddPersonEnabled] =
      base::Bind(&CoreOptionsHandler::IsUserUnsupervised,
                 base::Unretained(this));
}

void CoreOptionsHandler::InitializePage() {
  UpdateClearPluginLSOData();
  UpdatePepperFlashSettingsEnabled();
}

void CoreOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  GetStaticLocalizedValues(localized_strings);
}

void CoreOptionsHandler::GetStaticLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Main
  localized_strings->SetString("optionsPageTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));

  // Controlled settings bubble.
  localized_strings->SetString("controlledSettingPolicy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_POLICY));
  localized_strings->SetString("controlledSettingExtension",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_EXTENSION));
  localized_strings->SetString("controlledSettingExtensionWithName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_EXTENSION_WITH_NAME));
  localized_strings->SetString("controlledSettingManageExtension",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_MANAGE_EXTENSION));
  localized_strings->SetString("controlledSettingDisableExtension",
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE));
  localized_strings->SetString("controlledSettingRecommended",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_RECOMMENDED));
  localized_strings->SetString("controlledSettingHasRecommendation",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_HAS_RECOMMENDATION));
  localized_strings->SetString("controlledSettingFollowRecommendation",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTING_FOLLOW_RECOMMENDATION));
  localized_strings->SetString("controlledSettingsPolicy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTINGS_POLICY));
  localized_strings->SetString("controlledSettingsExtension",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTINGS_EXTENSION));
  localized_strings->SetString("controlledSettingsExtensionWithName",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONTROLLED_SETTINGS_EXTENSION_WITH_NAME));

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
                               chrome::kSettingsSearchHelpURL);

  // About
  localized_strings->SetString("aboutButton",
                               l10n_util::GetStringUTF16(IDS_ABOUT_BUTTON));

  // Common
  localized_strings->SetString("ok",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));
  localized_strings->SetString("learnMore",
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  localized_strings->SetString("close",
      l10n_util::GetStringUTF16(IDS_CLOSE));
  localized_strings->SetString("done",
      l10n_util::GetStringUTF16(IDS_DONE));
  localized_strings->SetString("deletableItemDeleteButtonTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_DELETABLE_ITEM_DELETE_BUTTON));
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

void CoreOptionsHandler::OnPreferenceChanged(PrefService* service,
                                             const std::string& pref_name) {
  if (pref_name == prefs::kClearPluginLSODataEnabled) {
    // This preference is stored in Local State, not in the user preferences.
    UpdateClearPluginLSOData();
    return;
  }
  if (pref_name == prefs::kPepperFlashSettingsEnabled) {
    UpdatePepperFlashSettingsEnabled();
    return;
  }
  NotifyPrefChanged(pref_name, std::string());
}

void CoreOptionsHandler::RegisterMessages() {
  registrar_.Init(Profile::FromWebUI(web_ui())->GetPrefs());
  local_state_registrar_.Init(g_browser_process->local_state());

  web_ui()->RegisterMessageCallback("coreOptionsInitialize",
      base::Bind(&CoreOptionsHandler::HandleInitialize,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onFinishedLoadingOptions",
      base::Bind(&CoreOptionsHandler::OnFinishedLoading,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("fetchPrefs",
      base::Bind(&CoreOptionsHandler::HandleFetchPrefs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("observePrefs",
      base::Bind(&CoreOptionsHandler::HandleObservePrefs,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setBooleanPref",
      base::Bind(&CoreOptionsHandler::HandleSetBooleanPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setIntegerPref",
      base::Bind(&CoreOptionsHandler::HandleSetIntegerPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setDoublePref",
      base::Bind(&CoreOptionsHandler::HandleSetDoublePref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setStringPref",
      base::Bind(&CoreOptionsHandler::HandleSetStringPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setURLPref",
      base::Bind(&CoreOptionsHandler::HandleSetURLPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setListPref",
      base::Bind(&CoreOptionsHandler::HandleSetListPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("clearPref",
      base::Bind(&CoreOptionsHandler::HandleClearPref,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("coreOptionsUserMetricsAction",
      base::Bind(&CoreOptionsHandler::HandleUserMetricsAction,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("disableExtension",
      base::Bind(&CoreOptionsHandler::HandleDisableExtension,
                 base::Unretained(this)));
}

void CoreOptionsHandler::HandleInitialize(const base::ListValue* args) {
  DCHECK(handlers_host_);
  handlers_host_->InitializeHandlers();
}

void CoreOptionsHandler::OnFinishedLoading(const base::ListValue* args) {
  DCHECK(handlers_host_);
  handlers_host_->OnFinishedLoading();
}

base::Value* CoreOptionsHandler::FetchPref(const std::string& pref_name) {
  return CreateValueForPref(pref_name, std::string());
}

void CoreOptionsHandler::ObservePref(const std::string& pref_name) {
  if (g_browser_process->local_state()->FindPreference(pref_name)) {
    local_state_registrar_.Add(
        pref_name,
        base::Bind(&CoreOptionsHandler::OnPreferenceChanged,
                   base::Unretained(this),
                   local_state_registrar_.prefs()));
  }
  // TODO(pneubeck): change this to if/else once kProxy is only used as a user
  // pref. Currently, it is both a user and a local state pref.
  if (Profile::FromWebUI(web_ui())->GetPrefs()->FindPreference(pref_name)) {
    registrar_.Add(
        pref_name,
        base::Bind(&CoreOptionsHandler::OnPreferenceChanged,
                   base::Unretained(this),
                   registrar_.prefs()));
  }
}

void CoreOptionsHandler::StopObservingPref(const std::string& pref_name) {
  if (g_browser_process->local_state()->FindPreference(pref_name))
    local_state_registrar_.Remove(pref_name);
  else
    registrar_.Remove(pref_name);
}

void CoreOptionsHandler::SetPref(const std::string& pref_name,
                                 const base::Value* value,
                                 const std::string& metric) {
  PrefService* pref_service = FindServiceForPref(pref_name);
  PrefChangeFilterMap::iterator iter = pref_change_filters_.find(pref_name);
  if (iter != pref_change_filters_.end()) {
    // Also check if the pref is user modifiable (don't even try to run the
    // filter function if the user is not allowed to change the pref).
    const PrefService::Preference* pref =
        pref_service->FindPreference(pref_name);
    if ((pref && !pref->IsUserModifiable()) || !iter->second.Run(value)) {
      // Reject the change; remind the page of the true value.
      NotifyPrefChanged(pref_name, std::string());
      return;
    }
  }

  switch (value->GetType()) {
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::STRING:
    case base::Value::Type::LIST:
      pref_service->Set(pref_name, *value);
      break;

    default:
      NOTREACHED();
      return;
  }

  ProcessUserMetric(value, metric);
}

void CoreOptionsHandler::ClearPref(const std::string& pref_name,
                                   const std::string& metric) {
  PrefService* pref_service = FindServiceForPref(pref_name);
  pref_service->ClearPref(pref_name);

  if (!metric.empty())
    content::RecordComputedAction(metric);
}

void CoreOptionsHandler::ProcessUserMetric(const base::Value* value,
                                           const std::string& metric) {
  if (metric.empty())
    return;

  std::string metric_string = metric;
  if (value->IsType(base::Value::Type::BOOLEAN)) {
    bool bool_value;
    CHECK(value->GetAsBoolean(&bool_value));
    metric_string += bool_value ? "_Enable" : "_Disable";
  }

  content::RecordComputedAction(metric_string);
}

void CoreOptionsHandler::NotifyPrefChanged(
    const std::string& pref_name,
    const std::string& controlling_pref_name) {
  std::unique_ptr<base::Value> value(
      CreateValueForPref(pref_name, controlling_pref_name));
  DispatchPrefChangeNotification(pref_name, std::move(value));
}

void CoreOptionsHandler::DispatchPrefChangeNotification(
    const std::string& name,
    std::unique_ptr<base::Value> value) {
  std::pair<PreferenceCallbackMap::const_iterator,
            PreferenceCallbackMap::const_iterator> range =
      pref_callback_map_.equal_range(name);
  base::ListValue result_value;
  result_value.AppendString(name);
  result_value.Append(std::move(value));
  for (PreferenceCallbackMap::const_iterator iter = range.first;
       iter != range.second; ++iter) {
    const std::string& callback_function = iter->second;
    web_ui()->CallJavascriptFunctionUnsafe(callback_function, result_value);
  }
}

base::Value* CoreOptionsHandler::CreateValueForPref(
    const std::string& pref_name,
    const std::string& controlling_pref_name) {
  const PrefService* pref_service = FindServiceForPref(pref_name);
  const PrefService::Preference* pref =
      pref_service->FindPreference(pref_name);
  if (!pref) {
    NOTREACHED();
    return base::Value::CreateNullValue().release();
  }
  const PrefService::Preference* controlling_pref =
      pref_service->FindPreference(controlling_pref_name);
  if (!controlling_pref)
    controlling_pref = pref;

  base::DictionaryValue* dict = new base::DictionaryValue;
  dict->Set("value", pref->GetValue()->DeepCopy());
  if (controlling_pref->IsManaged()) {
    dict->SetString("controlledBy", "policy");
  } else if (controlling_pref->IsExtensionControlled() &&
             CanSetExtensionControlledPrefValue(controlling_pref)) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ExtensionPrefValueMap* extension_pref_value_map =
        ExtensionPrefValueMapFactory::GetForBrowserContext(profile);
    std::string extension_id =
        extension_pref_value_map->GetExtensionControllingPref(
            controlling_pref->name());

    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      dict->SetString("controlledBy", "extension");
      dict->Set("extension",
                extensions::util::GetExtensionInfo(extension).release());
    }
  } else if (controlling_pref->IsRecommended()) {
    dict->SetString("controlledBy", "recommended");
  }

  const base::Value* recommended_value =
      controlling_pref->GetRecommendedValue();
  if (recommended_value)
    dict->Set("recommendedValue", recommended_value->DeepCopy());
  dict->SetBoolean("disabled", !controlling_pref->IsUserModifiable());
  return dict;
}

PrefService* CoreOptionsHandler::FindServiceForPref(
    const std::string& pref_name) {
  // Proxy is a peculiar case: on ChromeOS, settings exist in both user
  // prefs and local state, but chrome://settings should affect only user prefs.
  // Elsewhere the proxy settings are stored in local state.
  // See http://crbug.com/157147
  PrefService* user_prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  if (pref_name == proxy_config::prefs::kProxy)
#if defined(OS_CHROMEOS)
    return user_prefs;
#else
    return g_browser_process->local_state();
#endif

  // Find which PrefService contains the given pref. Pref names should not
  // be duplicated across services, however if they are, prefer the user's
  // prefs.
  if (user_prefs->FindPreference(pref_name))
    return user_prefs;

  if (g_browser_process->local_state()->FindPreference(pref_name))
    return g_browser_process->local_state();

  return user_prefs;
}

void CoreOptionsHandler::HandleFetchPrefs(const base::ListValue* args) {
  // First param is name of callback function, so, there needs to be at least
  // one more element for the actual preference identifier.
  DCHECK_GE(static_cast<int>(args->GetSize()), 2);

  // Get callback JS function name.
  const base::Value* callback;
  if (!args->Get(0, &callback) || !callback->IsType(base::Value::Type::STRING))
    return;

  base::string16 callback_function;
  if (!callback->GetAsString(&callback_function))
    return;

  // Get the list of name for prefs to build the response dictionary.
  base::DictionaryValue result_value;
  const base::Value* list_member;

  for (size_t i = 1; i < args->GetSize(); i++) {
    if (!args->Get(i, &list_member))
      break;

    if (!list_member->IsType(base::Value::Type::STRING))
      continue;

    std::string pref_name;
    if (!list_member->GetAsString(&pref_name))
      continue;

    result_value.Set(pref_name, FetchPref(pref_name));
  }
  web_ui()->CallJavascriptFunctionUnsafe(base::UTF16ToASCII(callback_function),
                                         result_value);
}

void CoreOptionsHandler::HandleObservePrefs(const base::ListValue* args) {
  // First param is name is JS callback function name, the rest are pref
  // identifiers that we are observing.
  DCHECK_GE(static_cast<int>(args->GetSize()), 2);

  // Get preference change callback function name.
  std::string callback_func_name;
  if (!args->GetString(0, &callback_func_name))
    return;

  // Get all other parameters - pref identifiers.
  for (size_t i = 1; i < args->GetSize(); i++) {
    const base::Value* list_member;
    if (!args->Get(i, &list_member))
      break;

    // Just ignore bad pref identifiers for now.
    std::string pref_name;
    if (!list_member->IsType(base::Value::Type::STRING) ||
        !list_member->GetAsString(&pref_name))
      continue;

    if (pref_callback_map_.find(pref_name) == pref_callback_map_.end())
      ObservePref(pref_name);

    pref_callback_map_.insert(
        PreferenceCallbackMap::value_type(pref_name, callback_func_name));
  }
}

void CoreOptionsHandler::HandleSetBooleanPref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_BOOLEAN);
}

void CoreOptionsHandler::HandleSetIntegerPref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_INTEGER);
}

void CoreOptionsHandler::HandleSetDoublePref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_DOUBLE);
}

void CoreOptionsHandler::HandleSetStringPref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_STRING);
}

void CoreOptionsHandler::HandleSetURLPref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_URL);
}

void CoreOptionsHandler::HandleSetListPref(const base::ListValue* args) {
  HandleSetPref(args, TYPE_LIST);
}

void CoreOptionsHandler::HandleSetPref(const base::ListValue* args,
                                       PrefType type) {
  DCHECK_GT(static_cast<int>(args->GetSize()), 1);

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  const base::Value* value;
  if (!args->Get(1, &value))
    return;

  std::unique_ptr<base::Value> temp_value;

  switch (type) {
    case TYPE_BOOLEAN:
      if (!value->IsType(base::Value::Type::BOOLEAN)) {
        NOTREACHED();
        return;
      }
      break;
    case TYPE_INTEGER: {
      // In JS all numbers are doubles.
      double double_value;
      if (!value->GetAsDouble(&double_value)) {
        NOTREACHED();
        return;
      }
      int int_value = static_cast<int>(double_value);
      temp_value.reset(new base::Value(int_value));
      value = temp_value.get();
      break;
    }
    case TYPE_DOUBLE:
      if (!value->IsType(base::Value::Type::DOUBLE)) {
        NOTREACHED();
        return;
      }
      break;
    case TYPE_STRING:
      if (!value->IsType(base::Value::Type::STRING)) {
        NOTREACHED();
        return;
      }
      break;
    case TYPE_URL: {
      std::string original;
      if (!value->GetAsString(&original)) {
        NOTREACHED();
        return;
      }
      GURL fixed = url_formatter::FixupURL(original, std::string());
      temp_value.reset(new base::StringValue(fixed.spec()));
      value = temp_value.get();
      break;
    }
    case TYPE_LIST: {
      // In case we have a List pref we got a JSON string.
      std::string json_string;
      if (!value->GetAsString(&json_string)) {
        NOTREACHED();
        return;
      }
      temp_value = base::JSONReader::Read(json_string);
      value = temp_value.get();
      if (!value || !value->IsType(base::Value::Type::LIST)) {
        NOTREACHED();
        return;
      }
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

void CoreOptionsHandler::HandleClearPref(const base::ListValue* args) {
  DCHECK_GT(static_cast<int>(args->GetSize()), 0);

  std::string pref_name;
  if (!args->GetString(0, &pref_name))
    return;

  std::string metric;
  if (args->GetSize() > 1) {
    if (!args->GetString(1, &metric))
      NOTREACHED();
  }

  ClearPref(pref_name, metric);
}

void CoreOptionsHandler::HandleUserMetricsAction(const base::ListValue* args) {
  std::string metric = base::UTF16ToUTF8(ExtractStringValue(args));
  if (!metric.empty())
    content::RecordComputedAction(metric);
}

void CoreOptionsHandler::HandleDisableExtension(const base::ListValue* args) {
  std::string extension_id;
  if (args->GetString(0, &extension_id)) {
    ExtensionService* extension_service = extensions::ExtensionSystem::Get(
        Profile::FromWebUI(web_ui()))->extension_service();
    DCHECK(extension_service);
    extension_service->DisableExtension(
        extension_id, extensions::Extension::DISABLE_USER_ACTION);
  } else {
    NOTREACHED();
  }
}

void CoreOptionsHandler::UpdateClearPluginLSOData() {
  base::Value enabled(plugin_status_pref_setter_.IsClearPluginLSODataEnabled());
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.OptionsPage.setClearPluginLSODataEnabled", enabled);
}

void CoreOptionsHandler::UpdatePepperFlashSettingsEnabled() {
  base::Value enabled(
      plugin_status_pref_setter_.IsPepperFlashSettingsEnabled());
  web_ui()->CallJavascriptFunctionUnsafe(
      "options.OptionsPage.setPepperFlashSettingsEnabled", enabled);
}

bool CoreOptionsHandler::IsUserUnsupervised(const base::Value* to_value) {
  return !Profile::FromWebUI(web_ui())->IsSupervised();
}

}  // namespace options
