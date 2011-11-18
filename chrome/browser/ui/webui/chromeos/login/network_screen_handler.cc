// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/status/input_method_menu.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/webui/web_ui.h"
#include "content/browser/webui/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/views/layout/fill_layout.h"
#include "views/widget/widget.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

namespace {

// Network screen id.
const char kNetworkScreen[] = "connect";

// JS API callbacks names.
const char kJsApiNetworkOnExit[] = "networkOnExit";
const char kJsApiNetworkOnLanguageChanged[] = "networkOnLanguageChanged";
const char kJsApiNetworkOnInputMethodChanged[] = "networkOnInputMethodChanged";

}  // namespace

namespace chromeos {

// NetworkScreenHandler, public: -----------------------------------------------

NetworkScreenHandler::NetworkScreenHandler()
    : screen_(NULL),
      is_continue_enabled_(false),
      show_on_init_(false) {
}

NetworkScreenHandler::~NetworkScreenHandler() {
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

  ShowScreen(kNetworkScreen, NULL);
}

void NetworkScreenHandler::Hide() {
}

void NetworkScreenHandler::ShowError(const string16& message) {
  scoped_ptr<Value> message_value(Value::CreateStringValue(message));
  web_ui_->CallJavascriptFunction("oobe.NetworkScreen.showError",
                                  *message_value);
}

void NetworkScreenHandler::ClearErrors() {
  web_ui_->CallJavascriptFunction("oobe.NetworkScreen.clearErrors");
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const string16& network_id) {
  // string16 connecting_label =
  //     l10n_util::GetStringFUTF16(IDS_NETWORK_SELECTION_CONNECTING,
  //                                network_id);
  // scoped_ptr<Value> connecting_value(Value::CreateBooleanValue(connecting));
  // scoped_ptr<Value> network_id_value(Value::CreateStringValue(network_id));
  // scoped_ptr<Value> connecting_label_value(
  //     Value::CreateStringValue(connecting_label));
  // web_ui_->CallJavascriptFunction("cr.ui.Oobe.showConnectingStatus",
  //                                 *connecting_value,
  //                                 *network_id_value,
  //                                 *connecting_label_value);
}

void NetworkScreenHandler::EnableContinue(bool enabled) {
  is_continue_enabled_ = enabled;
  if (!page_is_ready())
    return;

  scoped_ptr<Value> enabled_value(Value::CreateBooleanValue(enabled));
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.enableContinueButton",
                                  *enabled_value);
}

// NetworkScreenHandler, BaseScreenHandler implementation: --------------------

void NetworkScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("networkScreenTitle",
      l10n_util::GetStringUTF16(IDS_WELCOME_SCREEN_TITLE));
  localized_strings->SetString("selectLanguage",
      l10n_util::GetStringUTF16(IDS_LANGUAGE_SELECTION_SELECT));
  localized_strings->SetString("selectKeyboard",
      l10n_util::GetStringUTF16(IDS_KEYBOARD_SELECTION_SELECT));
  localized_strings->SetString("proxySettings",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));
  localized_strings->SetString("continueButton",
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_CONTINUE_BUTTON));
  localized_strings->Set("languageList", GetLanguageList());
  localized_strings->Set("inputMethodsList", GetInputMethods());
}

void NetworkScreenHandler::Initialize() {
  EnableContinue(is_continue_enabled_);
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

// NetworkScreenHandler, WebUIMessageHandler implementation: -------------------

void NetworkScreenHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(kJsApiNetworkOnExit,
      base::Bind(&NetworkScreenHandler::HandleOnExit,base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiNetworkOnLanguageChanged,
      base::Bind(&NetworkScreenHandler::HandleOnLanguageChanged,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback(kJsApiNetworkOnInputMethodChanged,
      base::Bind(&NetworkScreenHandler::HandleOnInputMethodChanged,
                 base::Unretained(this)));
}

// NetworkScreenHandler, private: ----------------------------------------------

void NetworkScreenHandler::HandleOnExit(const ListValue* args) {
  ClearErrors();
  screen_->OnContinuePressed();
}

void NetworkScreenHandler::HandleOnLanguageChanged(const ListValue* args) {
  DCHECK(args->GetSize() == 1);
  std::string locale;
  if (!args->GetString(0, &locale))
    NOTREACHED();

  // TODO(altimofeev): make language change async.
  LanguageSwitchMenu::SwitchLanguageAndEnableKeyboardLayouts(locale);

  DictionaryValue localized_strings;
  static_cast<OobeUI*>(web_ui_)->GetLocalizedStrings(&localized_strings);
  web_ui_->CallJavascriptFunction("cr.ui.Oobe.reloadContent",
                                  localized_strings);
}

void NetworkScreenHandler::HandleOnInputMethodChanged(const ListValue* args) {
  DCHECK(args->GetSize() == 1);
  std::string id;
  if (!args->GetString(0, &id))
    NOTREACHED();
  input_method::InputMethodManager::GetInstance()->ChangeInputMethod(id);
}

ListValue* NetworkScreenHandler::GetLanguageList() {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  // GetSupportedInputMethods() never returns NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  ListValue* languages_list =
      CrosLanguageOptionsHandler::GetLanguageList(*descriptors);
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

ListValue* NetworkScreenHandler::GetInputMethods() {
  ListValue* input_methods_list = new ListValue;
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  scoped_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveInputMethods());
  std::string current_input_method_id = manager->current_input_method().id();
  for (size_t i = 0; i < input_methods->size(); ++i) {
    DictionaryValue* input_method = new DictionaryValue;
    input_method->SetString("value", input_methods->at(i).id());
    input_method->SetString(
        "title", InputMethodMenu::GetTextForMenu(input_methods->at(i)));
    input_method->SetBoolean("selected",
        input_methods->at(i).id() == current_input_method_id);
    input_methods_list->Append(input_method);
  }
  return input_methods_list;
}

}  // namespace chromeos
