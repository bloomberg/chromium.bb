// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chromeos/ime/input_method_manager.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
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

}  // namespace

namespace chromeos {

// NetworkScreenHandler, public: -----------------------------------------------

NetworkScreenHandler::NetworkScreenHandler(CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      core_oobe_actor_(core_oobe_actor),
      is_continue_enabled_(false),
      show_on_init_(false) {
  DCHECK(core_oobe_actor_);
}

NetworkScreenHandler::~NetworkScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
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

  ShowScreen(OobeUI::kScreenOobeNetwork, NULL);
}

void NetworkScreenHandler::Hide() {
}

void NetworkScreenHandler::ShowError(const string16& message) {
  CallJS("showError", message);
}

void NetworkScreenHandler::ClearErrors() {
  if (page_is_ready())
    core_oobe_actor_->ClearErrors();
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const string16& network_id) {
  // string16 connecting_label =
  //     l10n_util::GetStringFUTF16(IDS_NETWORK_SELECTION_CONNECTING,
  //                                network_id);
  // CallJS("cr.ui.Oobe.showConnectingStatus",
  //        base::FundamentalValue(connecting),
  //        base::StringValue(network_id),
  //        base::StringValue(connecting_label_value));
}

void NetworkScreenHandler::EnableContinue(bool enabled) {
  is_continue_enabled_ = enabled;
  if (page_is_ready())
    CallJS("enableContinueButton", enabled);
}

// NetworkScreenHandler, BaseScreenHandler implementation: --------------------

void NetworkScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("networkScreenGreeting", IDS_WELCOME_SCREEN_GREETING);
  builder->Add("networkScreenTitle", IDS_WELCOME_SCREEN_TITLE);
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
  ClearErrors();
  if (screen_)
    screen_->OnContinuePressed();
}

void NetworkScreenHandler::HandleOnLanguageChanged(const std::string& locale) {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale)
    return;

  // TODO(altimofeev): make language change async.
  LanguageSwitchMenu::SwitchLanguageAndEnableKeyboardLayouts(locale);

  DictionaryValue localized_strings;
  static_cast<OobeUI*>(web_ui()->GetController())->GetLocalizedStrings(
      &localized_strings);
  core_oobe_actor_->ReloadContent(localized_strings);

  // Buttons are recreated, updated "Continue" button state.
  EnableContinue(is_continue_enabled_);
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

// static
ListValue* NetworkScreenHandler::GetLanguageList() {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  // GetSupportedInputMethods() never returns NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  ListValue* languages_list =
      options::CrosLanguageOptionsHandler::GetUILanguageList(*descriptors);
  for (size_t i = 0; i < languages_list->GetSize(); ++i) {
    DictionaryValue* language_info = NULL;
    if (!languages_list->GetDictionary(i, &language_info))
      NOTREACHED();

    std::string value;
    language_info->GetString("code", &value);
    std::string display_name;
    language_info->GetString("displayName", &display_name);
    std::string native_name;
    language_info->GetString("nativeDisplayName", &native_name);

    if (display_name != native_name)
      display_name = base::StringPrintf("%s - %s",
                                        display_name.c_str(),
                                        native_name.c_str());

    language_info->SetString("value", value);
    language_info->SetString("title", display_name);
    language_info->SetBoolean("selected", value == app_locale);
  }
  return languages_list;
}

// static
ListValue* NetworkScreenHandler::GetInputMethods() {
  ListValue* input_methods_list = new ListValue;
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveInputMethods());
  std::string current_input_method_id = manager->GetCurrentInputMethod().id();
  for (size_t i = 0; i < input_methods->size(); ++i) {
    const std::string ime_id = input_methods->at(i).id();
    DictionaryValue* input_method = new DictionaryValue;
    input_method->SetString("value", ime_id);
    input_method->SetString(
        "title",
        util->GetInputMethodLongName(input_methods->at(i)));
    input_method->SetBoolean("selected",
        ime_id == current_input_method_id);
    input_methods_list->Append(input_method);
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
