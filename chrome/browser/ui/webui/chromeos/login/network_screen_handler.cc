// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/idle_detector.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
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
      weak_ptr_factory_(this) {
  DCHECK(core_oobe_actor_);

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  manager->AddObserver(this);
}

NetworkScreenHandler::~NetworkScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  manager->RemoveObserver(this);
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
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  const std::string selected_language = selected_language_code_.empty() ?
      application_locale : selected_language_code_;
  const std::string selected_input_method =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetCurrentInputMethod()
          .id();

  dict->Set("languageList",
            GetUILanguageList(NULL, selected_language).release());
  dict->Set("inputMethodsList",
            GetAndActivateLoginKeyboardLayouts(
                application_locale, selected_input_method).release());
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
  core_oobe_actor_->StopDemoModeDetection();
  ClearErrors();
  if (screen_)
    screen_->OnContinuePressed();
}

struct NetworkScreenHandlerOnLanguageChangedCallbackData {
  explicit NetworkScreenHandlerOnLanguageChangedCallbackData(
      const base::WeakPtr<NetworkScreenHandler>& handler)
      : handler(handler) {}

  base::WeakPtr<NetworkScreenHandler> handler;

  // Block UI while resource bundle is being reloaded.
  chromeos::InputEventsBlocker input_events_blocker;
};

// static
void NetworkScreenHandler::OnLanguageChangedCallback(
    scoped_ptr<NetworkScreenHandlerOnLanguageChangedCallbackData> context,
    const std::string& requested_locale,
    const std::string& loaded_locale,
    const bool success) {
  if (!context || !context->handler)
    return;

  NetworkScreenHandler* const self = context->handler.get();

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
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->ChangeInputMethod(id, false /* show_message */);
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

void NetworkScreenHandler::InputMethodChanged(
    input_method::InputMethodManager* manager, bool show_message) {
  CallJS("setInputMethod",
         manager->GetActiveIMEState()->GetCurrentInputMethod().id());
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
