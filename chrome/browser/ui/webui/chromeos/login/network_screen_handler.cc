// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/idle_detector.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const char kJsScreenPath[] = "login.NetworkScreen";

// JS API callbacks names.
const char kJsApiNetworkOnExit[] = "networkOnExit";
const char kJsApiNetworkOnLanguageChanged[] = "networkOnLanguageChanged";
const char kJsApiNetworkOnInputMethodChanged[] = "networkOnInputMethodChanged";
const char kJsApiNetworkOnTimezoneChanged[] = "networkOnTimezoneChanged";

// Returns true if element was inserted.
bool InsertString(const std::string& str, std::set<std::string>& to) {
  const std::pair<std::set<std::string>::iterator, bool> result =
      to.insert(str);
  return result.second;
}

void AddOptgroupOtherLayouts(base::ListValue* input_methods_list) {
  base::DictionaryValue* optgroup = new base::DictionaryValue;
  optgroup->SetString(
      "optionGroupName",
      l10n_util::GetStringUTF16(IDS_OOBE_OTHER_KEYBOARD_LAYOUTS));
  input_methods_list->Append(optgroup);
}

// For "UI Language" drop-down menu at OOBE screen we need to decide which
// entry to mark "selected". If user has just selected "requested_locale",
// but "loaded_locale" was actually loaded, we mark original user choice
// "selected" only if loaded_locale is a backup for "requested_locale".
std::string CalculateSelectedLanguage(const std::string& requested_locale,
                                      const std::string& loaded_locale) {

  std::string resolved_locale;
  if (!l10n_util::CheckAndResolveLocale(requested_locale, &resolved_locale))
    return loaded_locale;

  if (resolved_locale == loaded_locale)
    return requested_locale;

  return loaded_locale;
}

}  // namespace

namespace chromeos {

// NetworkScreenHandler, public: -----------------------------------------------

NetworkScreenHandler::NetworkScreenHandler(CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      core_oobe_actor_(core_oobe_actor),
      is_continue_enabled_(false),
      show_on_init_(false),
      should_reinitialize_language_keyboard_list_(false),
      weak_ptr_factory_(this) {
  DCHECK(core_oobe_actor_);

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  manager->AddObserver(this);
  manager->GetComponentExtensionIMEManager()->AddObserver(this);
}

NetworkScreenHandler::~NetworkScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  manager->RemoveObserver(this);
  manager->GetComponentExtensionIMEManager()->RemoveObserver(this);
}

// NetworkScreenHandler, NetworkScreenActor implementation: --------------------

void NetworkScreenHandler::SetDelegate(NetworkScreenActor::Delegate* screen) {
  screen_ = screen;
}

void NetworkScreenHandler::PrepareToShow() {
}

void NetworkScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  // Here we should handle default locales, for which we do not have UI
  // resources. This would load fallback, but properly show "selected" locale
  // in the UI.
  if (selected_language_code_.empty()) {
    const StartupCustomizationDocument* startup_manifest =
        StartupCustomizationDocument::GetInstance();
    HandleOnLanguageChanged(startup_manifest->initial_locale_default());
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_actor_)
      core_oobe_actor_->ShowDeviceResetScreen();
    return;
  }

  // Make sure all our network technologies are turned on. On OOBE, the user
  // should be able to select any of the available networks on the device.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->SetTechnologyEnabled(NetworkTypePattern::NonVirtual(),
                                true,
                                chromeos::network_handler::ErrorCallback());
  ShowScreen(OobeUI::kScreenOobeNetwork, NULL);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableDemoMode))
    return;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    std::string track;
    // We're running on an actual device; if we cannot find our release track
    // value or if the track contains "testimage", don't start demo mode.
    if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &track) ||
        track.find("testimage") != std::string::npos)
      return;
  }
  core_oobe_actor_->InitDemoModeDetection();
}

void NetworkScreenHandler::Hide() {
}

void NetworkScreenHandler::ShowError(const base::string16& message) {
  CallJS("showError", message);
}

void NetworkScreenHandler::ClearErrors() {
  if (page_is_ready())
    core_oobe_actor_->ClearErrors();
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const base::string16& network_id) {
}

void NetworkScreenHandler::EnableContinue(bool enabled) {
  is_continue_enabled_ = enabled;
  if (page_is_ready())
    CallJS("enableContinueButton", enabled);
}

// NetworkScreenHandler, BaseScreenHandler implementation: --------------------

void NetworkScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    builder->Add("networkScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
  else
    builder->Add("networkScreenGreeting", IDS_WELCOME_SCREEN_GREETING);

  builder->Add("networkScreenTitle", IDS_WELCOME_SCREEN_TITLE);
  builder->Add("networkScreenAccessibleTitle",
               IDS_NETWORK_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("selectLanguage", IDS_LANGUAGE_SELECTION_SELECT);
  builder->Add("selectKeyboard", IDS_KEYBOARD_SELECTION_SELECT);
  builder->Add("selectNetwork", IDS_NETWORK_SELECTION_SELECT);
  builder->Add("selectTimezone", IDS_OPTIONS_SETTINGS_TIMEZONE_DESCRIPTION);
  builder->Add("proxySettings", IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON);
  builder->Add("continueButton", IDS_NETWORK_SELECTION_CONTINUE_BUTTON);
}

void NetworkScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
  dict->Set("languageList", GetLanguageList());
  dict->Set("inputMethodsList", GetInputMethods());
  dict->Set("timezoneList", GetTimezoneList());
}

void NetworkScreenHandler::Initialize() {
  EnableContinue(is_continue_enabled_);
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }

  if (should_reinitialize_language_keyboard_list_) {
    should_reinitialize_language_keyboard_list_ = false;
    ReloadLocalizedContent();
  }

  timezone_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kSystemTimezone,
      base::Bind(&NetworkScreenHandler::OnSystemTimezoneChanged,
                 base::Unretained(this)));
  OnSystemTimezoneChanged();
}

// NetworkScreenHandler, WebUIMessageHandler implementation: -------------------

void NetworkScreenHandler::RegisterMessages() {
  AddCallback(kJsApiNetworkOnExit, &NetworkScreenHandler::HandleOnExit);
  AddCallback(kJsApiNetworkOnLanguageChanged,
              &NetworkScreenHandler::HandleOnLanguageChanged);
  AddCallback(kJsApiNetworkOnInputMethodChanged,
              &NetworkScreenHandler::HandleOnInputMethodChanged);
  AddCallback(kJsApiNetworkOnTimezoneChanged,
              &NetworkScreenHandler::HandleOnTimezoneChanged);
}


// NetworkScreenHandler, private: ----------------------------------------------

void NetworkScreenHandler::HandleOnExit() {
  core_oobe_actor_->StopDemoModeDetection();
  ClearErrors();
  if (screen_)
    screen_->OnContinuePressed();
}

struct NetworkScreenHandlerOnLanguageChangedCallbackData {
  explicit NetworkScreenHandlerOnLanguageChangedCallbackData(
      base::WeakPtr<NetworkScreenHandler>& handler)
      : handler_(handler) {}

  base::WeakPtr<NetworkScreenHandler> handler_;

  // Block UI while resource bundle is being reloaded.
  chromeos::InputEventsBlocker input_events_blocker;
};

// static
void NetworkScreenHandler::OnLanguageChangedCallback(
    scoped_ptr<NetworkScreenHandlerOnLanguageChangedCallbackData> context,
    const std::string& requested_locale,
    const std::string& loaded_locale,
    const bool success) {
  if (!context or !context->handler_)
    return;

  NetworkScreenHandler* const self = context->handler_.get();

  if (success) {
    if (requested_locale == loaded_locale) {
      self->selected_language_code_ = requested_locale;
    } else {
      self->selected_language_code_ =
          CalculateSelectedLanguage(requested_locale, loaded_locale);
    }
  } else {
    self->selected_language_code_ = loaded_locale;
  }

  self->ReloadLocalizedContent();

  // We still do not have device owner, so owner settings are not applied.
  // But Guest session can be started before owner is created, so we need to
  // save locale settings directly here.
  g_browser_process->local_state()->SetString(prefs::kApplicationLocale,
                                              self->selected_language_code_);

  AccessibilityManager::Get()->OnLocaleChanged();
}

void NetworkScreenHandler::HandleOnLanguageChanged(const std::string& locale) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale)
    return;

  base::WeakPtr<NetworkScreenHandler> weak_self =
      weak_ptr_factory_.GetWeakPtr();
  scoped_ptr<NetworkScreenHandlerOnLanguageChangedCallbackData> callback_data(
      new NetworkScreenHandlerOnLanguageChangedCallbackData(weak_self));
  scoped_ptr<locale_util::SwitchLanguageCallback> callback(
      new locale_util::SwitchLanguageCallback(
          base::Bind(&NetworkScreenHandler::OnLanguageChangedCallback,
                     base::Passed(callback_data.Pass()))));
  locale_util::SwitchLanguage(locale,
                              true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */,
                              callback.Pass());
}

void NetworkScreenHandler::HandleOnInputMethodChanged(const std::string& id) {
  input_method::InputMethodManager::Get()->ChangeInputMethod(id);
}

void NetworkScreenHandler::HandleOnTimezoneChanged(
    const std::string& timezone_id) {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  if (current_timezone_id == timezone_id)
    return;

  CrosSettings::Get()->SetString(kSystemTimezone, timezone_id);
}

void NetworkScreenHandler::OnSystemTimezoneChanged() {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);
  CallJS("setTimezone", current_timezone_id);
}

base::ListValue* NetworkScreenHandler::GetLanguageList() {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  ComponentExtensionIMEManager* comp_manager =
      manager->GetComponentExtensionIMEManager();
  input_method::InputMethodDescriptors descriptors;
  if (comp_manager->IsInitialized())
    descriptors = comp_manager->GetXkbIMEAsInputMethodDescriptor();
  base::ListValue* languages_list =
      options::CrosLanguageOptionsHandler::GetUILanguageList(descriptors);
  for (size_t i = 0; i < languages_list->GetSize(); ++i) {
    base::DictionaryValue* language_info = NULL;
    if (!languages_list->GetDictionary(i, &language_info))
      NOTREACHED();

    std::string value;
    language_info->GetString("code", &value);
    std::string display_name;
    language_info->GetString("displayName", &display_name);
    std::string native_name;
    language_info->GetString("nativeDisplayName", &native_name);

    // If it's option group divider, add field name.
    if (value == options::kVendorOtherLanguagesListDivider) {
      language_info->SetString(
          "optionGroupName",
          l10n_util::GetStringUTF16(IDS_OOBE_OTHER_LANGUAGES));
    }
    if (display_name != native_name) {
      display_name = base::StringPrintf("%s - %s",
                                        display_name.c_str(),
                                        native_name.c_str());
    }

    language_info->SetString("value", value);
    language_info->SetString("title", display_name);
    if (selected_language_code_.empty()) {
      if (value == app_locale)
        language_info->SetBoolean("selected", true);
    } else {
      if (value == selected_language_code_)
        language_info->SetBoolean("selected", true);
    }
  }
  return languages_list;
}

base::DictionaryValue* CreateInputMethodsEntry(
    const input_method::InputMethodDescriptor& method,
    const std::string current_input_method_id) {
  input_method::InputMethodUtil* util =
      input_method::InputMethodManager::Get()->GetInputMethodUtil();
  const std::string& ime_id = method.id();
  scoped_ptr<base::DictionaryValue> input_method(new base::DictionaryValue);
  input_method->SetString("value", ime_id);
  input_method->SetString("title", util->GetInputMethodLongName(method));
  input_method->SetBoolean("selected", ime_id == current_input_method_id);
  return input_method.release();
}

void NetworkScreenHandler::OnImeComponentExtensionInitialized() {
  // Refreshes the language and keyboard list once the component extension
  // IMEs are initialized.
  if (page_is_ready())
    ReloadLocalizedContent();
  else
    should_reinitialize_language_keyboard_list_ = true;
}

void NetworkScreenHandler::InputMethodChanged(
    input_method::InputMethodManager* manager, bool show_message) {
  CallJS("setInputMethod", manager->GetCurrentInputMethod().id());
}

void NetworkScreenHandler::ReloadLocalizedContent() {
  base::DictionaryValue localized_strings;
  static_cast<OobeUI*>(web_ui()->GetController())
      ->GetLocalizedStrings(&localized_strings);
  core_oobe_actor_->ReloadContent(localized_strings);

  // Buttons are recreated, updated "Continue" button state.
  EnableContinue(is_continue_enabled_);
}

// static
base::ListValue* NetworkScreenHandler::GetInputMethods() {
  base::ListValue* input_methods_list = new base::ListValue;
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  ComponentExtensionIMEManager* comp_manager =
      manager->GetComponentExtensionIMEManager();
  if (!comp_manager->IsInitialized()) {
    input_method::InputMethodDescriptor fallback =
        util->GetFallbackInputMethodDescriptor();
    input_methods_list->Append(
        CreateInputMethodsEntry(fallback, fallback.id()));
    return input_methods_list;
  }

  const std::vector<std::string>& hardware_login_input_methods =
      util->GetHardwareLoginInputMethodIds();
  manager->EnableLoginLayouts(g_browser_process->GetApplicationLocale(),
                              hardware_login_input_methods);

  scoped_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveInputMethods());
  const std::string current_input_method_id =
      manager->GetCurrentInputMethod().id();
  std::set<std::string> input_methods_added;

  for (std::vector<std::string>::const_iterator i =
           hardware_login_input_methods.begin();
       i != hardware_login_input_methods.end();
       ++i) {
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*i);
    DCHECK(ime != NULL);
    // Do not crash in case of misconfiguration.
    if (ime != NULL) {
      input_methods_added.insert(*i);
      input_methods_list->Append(
          CreateInputMethodsEntry(*ime, current_input_method_id));
    }
  }

  bool optgroup_added = false;
  for (size_t i = 0; i < input_methods->size(); ++i) {
    // Makes sure the id is in legacy xkb id format.
    const std::string& ime_id = (*input_methods)[i].id();
    if (!InsertString(ime_id, input_methods_added))
      continue;
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list);
    }
    input_methods_list->Append(
        CreateInputMethodsEntry((*input_methods)[i], current_input_method_id));
  }
  // "xkb:us::eng" should always be in the list of available layouts.
  const std::string us_keyboard_id =
      util->GetFallbackInputMethodDescriptor().id();
  if (input_methods_added.find(us_keyboard_id) == input_methods_added.end()) {
    const input_method::InputMethodDescriptor* us_eng_descriptor =
        util->GetInputMethodDescriptorFromId(us_keyboard_id);
    DCHECK(us_eng_descriptor != NULL);
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list);
    }
    input_methods_list->Append(
        CreateInputMethodsEntry(*us_eng_descriptor, current_input_method_id));
  }
  return input_methods_list;
}

// static
base::ListValue* NetworkScreenHandler::GetTimezoneList() {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);

  scoped_ptr<base::ListValue> timezone_list(new base::ListValue);
  scoped_ptr<base::ListValue> timezones = system::GetTimezoneList().Pass();
  for (size_t i = 0; i < timezones->GetSize(); ++i) {
    const base::ListValue* timezone = NULL;
    CHECK(timezones->GetList(i, &timezone));

    std::string timezone_id;
    CHECK(timezone->GetString(0, &timezone_id));

    std::string timezone_name;
    CHECK(timezone->GetString(1, &timezone_name));

    scoped_ptr<base::DictionaryValue> timezone_option(
        new base::DictionaryValue);
    timezone_option->SetString("value", timezone_id);
    timezone_option->SetString("title", timezone_name);
    timezone_option->SetBoolean("selected", timezone_id == current_timezone_id);
    timezone_list->Append(timezone_option.release());
  }

  return timezone_list.release();
}

}  // namespace chromeos
