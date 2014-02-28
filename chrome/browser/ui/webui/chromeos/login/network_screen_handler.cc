// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/idle_detector.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/input_events_blocker.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/ime/input_method_manager.h"
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

const char kUSlayout[] = "xkb:us::eng";

const int kDerelectDetectionTimeoutSeconds = 8 * 60 * 60; // 8 hours.
const int kDerelectIdleTimeoutSeconds = 5 * 60; // 5 minutes.
const int kOobeTimerUpdateIntervalSeconds = 5 * 60; // 5 minutes.

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
  SetupTimeouts();
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

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableDemoMode))
    return;

  if (IsDerelict())
    StartIdleDetection();
  else
    StartOobeTimer();
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
  // base::string16 connecting_label =
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
  if (system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation())
    builder->Add("networkScreenGreeting", IDS_REMORA_CONFIRM_MESSAGE);
  else
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


// static
void NetworkScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kTimeOnOobe, 0);
}

// NetworkScreenHandler, private: ----------------------------------------------

void NetworkScreenHandler::HandleOnExit() {
  detector_.reset();
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
    const std::string& /*requested locale*/,
    const std::string& /*loaded_locale*/,
    const bool /*success*/) {
  if (!context or !context->handler_)
    return;

  NetworkScreenHandler* const self = context->handler_.get();

  base::DictionaryValue localized_strings;
  static_cast<OobeUI*>(self->web_ui()->GetController())
      ->GetLocalizedStrings(&localized_strings);
  self->core_oobe_actor_->ReloadContent(localized_strings);

  // Buttons are recreated, updated "Continue" button state.
  self->EnableContinue(self->is_continue_enabled_);
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

void NetworkScreenHandler::StartIdleDetection() {
  if (!detector_.get()) {
    detector_.reset(
        new IdleDetector(base::Closure(),
                         base::Bind(&NetworkScreenHandler::OnIdle,
                                    weak_ptr_factory_.GetWeakPtr())));
  }
  detector_->Start(derelict_idle_timeout_);
}

void NetworkScreenHandler::StartOobeTimer() {
  oobe_timer_.Start(FROM_HERE,
                    oobe_timer_update_interval_,
                    this,
                    &NetworkScreenHandler::OnOobeTimerUpdate);
}

void NetworkScreenHandler::OnIdle() {
  LoginDisplayHost* host = LoginDisplayHostImpl::default_host();
  host->StartDemoAppLaunch();
}

void NetworkScreenHandler::OnOobeTimerUpdate() {
  time_on_oobe_ += oobe_timer_update_interval_;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInt64(prefs::kTimeOnOobe, time_on_oobe_.InSeconds());

  if (IsDerelict()) {
    oobe_timer_.Stop();
    StartIdleDetection();
  }
}

void NetworkScreenHandler::SetupTimeouts() {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  DCHECK(cmdline);

  PrefService* prefs = g_browser_process->local_state();
  time_on_oobe_ =
      base::TimeDelta::FromSeconds(prefs->GetInt64(prefs::kTimeOnOobe));

  int derelict_detection_timeout;
  if (!cmdline->HasSwitch(switches::kDerelictDetectionTimeout) ||
      !base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kDerelictDetectionTimeout),
          &derelict_detection_timeout)) {
    derelict_detection_timeout = kDerelectDetectionTimeoutSeconds;
  }
  derelict_detection_timeout_ =
      base::TimeDelta::FromSeconds(derelict_detection_timeout);

  int derelict_idle_timeout;
  if (!cmdline->HasSwitch(switches::kDerelictIdleTimeout) ||
      !base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kDerelictIdleTimeout),
          &derelict_idle_timeout)) {
    derelict_idle_timeout = kDerelectIdleTimeoutSeconds;
  }
  derelict_idle_timeout_ = base::TimeDelta::FromSeconds(derelict_idle_timeout);


  int oobe_timer_update_interval;
  if (!cmdline->HasSwitch(switches::kOobeTimerInterval) ||
      !base::StringToInt(
          cmdline->GetSwitchValueASCII(switches::kOobeTimerInterval),
          &oobe_timer_update_interval)) {
    oobe_timer_update_interval = kOobeTimerUpdateIntervalSeconds;
  }
  oobe_timer_update_interval_ =
      base::TimeDelta::FromSeconds(oobe_timer_update_interval);

  // In case we'd be derelict before our timer is set to trigger, reduce
  // the interval so we check again when we're scheduled to go derelict.
  oobe_timer_update_interval_ =
      std::max(std::min(oobe_timer_update_interval_,
                        derelict_detection_timeout_ - time_on_oobe_),
               base::TimeDelta::FromSeconds(0));
}

bool NetworkScreenHandler::IsDerelict() {
  return time_on_oobe_ >= derelict_detection_timeout_;
}

// static
base::ListValue* NetworkScreenHandler::GetLanguageList() {
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  // GetSupportedInputMethods() never returns NULL.
  scoped_ptr<input_method::InputMethodDescriptors> descriptors(
      manager->GetSupportedInputMethods());
  base::ListValue* languages_list =
      options::CrosLanguageOptionsHandler::GetUILanguageList(*descriptors);
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
    language_info->SetBoolean("selected", value == app_locale);
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

// static
base::ListValue* NetworkScreenHandler::GetInputMethods() {
  base::ListValue* input_methods_list = new base::ListValue;
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();
  input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_ptr<input_method::InputMethodDescriptors> input_methods(
      manager->GetActiveInputMethods());
  std::string current_input_method_id = manager->GetCurrentInputMethod().id();
  const std::vector<std::string>& hardware_login_input_methods =
      util->GetHardwareLoginInputMethodIds();
  std::set<std::string> input_methods_added;

  for (std::vector<std::string>::const_iterator i =
           hardware_login_input_methods.begin();
       i != hardware_login_input_methods.end();
       ++i) {
    input_methods_added.insert(*i);
    const input_method::InputMethodDescriptor* ime =
        util->GetInputMethodDescriptorFromId(*i);
    DCHECK(ime != NULL);
    // Do not crash in case of misconfiguration.
    if (ime != NULL) {
      input_methods_list->Append(
          CreateInputMethodsEntry(*ime, current_input_method_id));
    }
  }

  bool optgroup_added = false;
  for (size_t i = 0; i < input_methods->size(); ++i) {
    const std::string& ime_id = input_methods->at(i).id();
    if (!InsertString(ime_id, input_methods_added))
      continue;
    if (!optgroup_added) {
      optgroup_added = true;
      AddOptgroupOtherLayouts(input_methods_list);
    }
    input_methods_list->Append(
        CreateInputMethodsEntry(input_methods->at(i), current_input_method_id));
  }
  // "xkb:us::eng" should always be in the list of available layouts.
  if (input_methods_added.count(kUSlayout) == 0) {
    const input_method::InputMethodDescriptor* us_eng_descriptor =
        util->GetInputMethodDescriptorFromId(kUSlayout);
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
