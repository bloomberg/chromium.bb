// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/network_screen_actor.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/gfx/geometry/point.h"

namespace chromeos {

class CoreOobeActor;
class IdleDetector;
class InputEventsBlocker;

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkScreenActor,
                             public BaseScreenHandler,
                             public input_method::InputMethodManager::Observer {
 public:
  explicit NetworkScreenHandler(CoreOobeActor* core_oobe_actor);
  virtual ~NetworkScreenHandler();

 private:
  // NetworkScreenActor implementation:
  virtual void SetDelegate(NetworkScreenActor::Delegate* screen) override;
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void ShowError(const base::string16& message) override;
  virtual void ClearErrors() override;
  virtual void ShowConnectingStatus(bool connecting,
                                    const base::string16& network_id) override;
  virtual void EnableContinue(bool enabled) override;
  virtual std::string GetApplicationLocale() const override;
  virtual std::string GetInputMethod() const override;
  virtual std::string GetTimezone() const override;
  virtual void SetApplicationLocale(const std::string& locale) override;
  virtual void SetInputMethod(const std::string& input_method) override;
  virtual void SetTimezone(const std::string& timezone) override;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void GetAdditionalParameters(base::DictionaryValue* dict) override;
  virtual void Initialize() override;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() override;

  // InputMethodManager::Observer implementation:
  virtual void InputMethodChanged(input_method::InputMethodManager* manager,
                                  bool show_message) override;

 private:
  // Reloads localized contents.
  void ReloadLocalizedContent();

  // Handles moving off the screen.
  void HandleOnExit();

  // Async callback after ReloadResourceBundle(locale) completed.
  void OnLanguageChangedCallback(
      const InputEventsBlocker* input_events_blocker,
      const locale_util::LanguageSwitchResult& result);

  // Callback when the system timezone settings is changed.
  void OnSystemTimezoneChanged();

  // Returns available timezones. Caller gets the ownership.
  static base::ListValue* GetTimezoneList();

  // Starts resolving language list on BlockingPool.
  void ScheduleResolveLanguageList(
      scoped_ptr<locale_util::LanguageSwitchResult> language_switch_result);

  // Callback for chromeos::ResolveUILanguageList() (from l10n_util).
  void OnLanguageListResolved(scoped_ptr<base::ListValue> new_language_list,
                              std::string new_language_list_locale,
                              std::string new_selected_language);

  NetworkScreenActor::Delegate* screen_;
  CoreOobeActor* core_oobe_actor_;

  bool is_continue_enabled_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Position of the network control.
  gfx::Point network_control_pos_;

  scoped_ptr<CrosSettings::ObserverSubscription> timezone_subscription_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  std::string locale_;
  std::string input_method_;
  std::string timezone_;

  // Creation of language list happens on Blocking Pool, so we cache resolved
  // data.
  std::string language_list_locale_;
  scoped_ptr<base::ListValue> language_list_;

  base::WeakPtrFactory<NetworkScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
