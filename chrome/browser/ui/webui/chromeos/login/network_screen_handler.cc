// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/idle_detector.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/screens/network_model.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const char kJsScreenPath[] = "login.NetworkScreen";

}  // namespace

namespace chromeos {

// NetworkScreenHandler, public: -----------------------------------------------

NetworkScreenHandler::NetworkScreenHandler(CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      core_oobe_actor_(core_oobe_actor),
      model_(nullptr),
      show_on_init_(false) {
  DCHECK(core_oobe_actor_);
}

NetworkScreenHandler::~NetworkScreenHandler() {
  if (model_)
    model_->OnViewDestroyed(this);
}

// NetworkScreenHandler, NetworkScreenActor implementation: --------------------

void NetworkScreenHandler::PrepareToShow() {
}

void NetworkScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_actor_)
      core_oobe_actor_->ShowDeviceResetScreen();

    return;
  } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
    if (core_oobe_actor_)
      core_oobe_actor_->ShowEnableDebuggingScreen();

    return;
  }

  // Make sure all our network technologies are turned on. On OOBE, the user
  // should be able to select any of the available networks on the device.
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->SetTechnologyEnabled(NetworkTypePattern::NonVirtual(),
                                true,
                                chromeos::network_handler::ErrorCallback());

  base::DictionaryValue network_screen_params;
  network_screen_params.SetBoolean("isDeveloperMode",
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode));
  ShowScreen(OobeUI::kScreenOobeNetwork, &network_screen_params);
  core_oobe_actor_->InitDemoModeDetection();
}

void NetworkScreenHandler::Hide() {
}

void NetworkScreenHandler::Bind(NetworkModel& model) {
  model_ = &model;
  BaseScreenHandler::SetBaseScreen(model_);
}

void NetworkScreenHandler::Unbind() {
  model_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void NetworkScreenHandler::ShowError(const base::string16& message) {
  CallJS("showError", message);
}

void NetworkScreenHandler::ClearErrors() {
  if (page_is_ready())
    core_oobe_actor_->ClearErrors();
}

void NetworkScreenHandler::StopDemoModeDetection() {
  core_oobe_actor_->StopDemoModeDetection();
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const base::string16& network_id) {
}

void NetworkScreenHandler::ReloadLocalizedContent() {
  base::DictionaryValue localized_strings;
  static_cast<OobeUI*>(web_ui()->GetController())
      ->GetLocalizedStrings(&localized_strings);
  core_oobe_actor_->ReloadContent(localized_strings);
}

// NetworkScreenHandler, BaseScreenHandler implementation: --------------------

void NetworkScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
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
  builder->Add("debuggingFeaturesLink", IDS_NETWORK_ENABLE_DEV_FEATURES_LINK);
}

void NetworkScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
  const std::string application_locale =
      g_browser_process->GetApplicationLocale();
  const std::string selected_input_method =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetCurrentInputMethod()
          .id();

  scoped_ptr<base::ListValue> language_list;
  if (model_) {
    if (model_->GetLanguageList() &&
        model_->GetLanguageListLocale() == application_locale) {
      language_list.reset(model_->GetLanguageList()->DeepCopy());
    } else {
      model_->UpdateLanguageList();
    }
  }

  if (!language_list.get())
    language_list.reset(GetMinimalUILanguageList().release());

  // GetAdditionalParameters() is called when OOBE language is updated.
  // This happens in three different cases:
  //
  // 1) User selects new locale on OOBE screen. We need to sync active input
  // methods with locale, so EnableLoginLayouts() is needed.
  //
  // 2) This is signin to public session. User has selected some locale & input
  // method on "Public Session User POD". After "Login" button is pressed,
  // new user session is created, locale & input method are changed (both
  // asynchronously).
  // But after public user session is started, "Terms of Service" dialog is
  // shown. It is a part of OOBE UI screens, so it initiates reload of UI
  // strings in new locale. It also happens asynchronously, that leads to race
  // between "locale change", "input method change" and
  // "EnableLoginLayouts()".  This way EnableLoginLayouts() happens after user
  // input method has been changed, resetting input method to hardware default.
  //
  // So we need to disable activation of login layouts if we are already in
  // active user session.
  //
  // 3) This is the bootstrapping process for the remora/"Slave" device. The
  // locale & input of the remora/"Slave" device is set up by a shark/"Master"
  // device. In this case we don't want EnableLoginLayout() to reset the input
  // method to the hardware default method.
  const bool is_remora = g_browser_process->platform_part()
                             ->browser_policy_connector_chromeos()
                             ->GetDeviceCloudPolicyManager()
                             ->IsRemoraRequisition();

  const bool is_slave = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kOobeBootstrappingSlave);

  const bool enable_layouts =
      !user_manager::UserManager::Get()->IsUserLoggedIn() && !is_slave &&
      !is_remora;

  dict->Set("languageList", language_list.release());
  dict->Set(
      "inputMethodsList",
      GetAndActivateLoginKeyboardLayouts(
          application_locale, selected_input_method, enable_layouts).release());
  dict->Set("timezoneList", GetTimezoneList());
}

void NetworkScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }

  // Reload localized strings if they are already resolved.
  if (model_ && model_->GetLanguageList())
    ReloadLocalizedContent();
}

// NetworkScreenHandler, private: ----------------------------------------------

// static
base::ListValue* NetworkScreenHandler::GetTimezoneList() {
  std::string current_timezone_id;
  CrosSettings::Get()->GetString(kSystemTimezone, &current_timezone_id);

  scoped_ptr<base::ListValue> timezone_list(new base::ListValue);
  scoped_ptr<base::ListValue> timezones = system::GetTimezoneList();
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
