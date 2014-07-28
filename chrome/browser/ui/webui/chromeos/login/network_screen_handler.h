// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/screens/network_screen_actor.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/gfx/point.h"

namespace chromeos {

class CoreOobeActor;
class IdleDetector;

struct NetworkScreenHandlerOnLanguageChangedCallbackData;

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkScreenActor,
                             public BaseScreenHandler,
                             public input_method::InputMethodManager::Observer {
 public:
  explicit NetworkScreenHandler(CoreOobeActor* core_oobe_actor);
  virtual ~NetworkScreenHandler();

  // NetworkScreenActor implementation:
  virtual void SetDelegate(NetworkScreenActor::Delegate* screen) OVERRIDE;
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowError(const base::string16& message) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;
  virtual void ShowConnectingStatus(bool connecting,
                                    const base::string16& network_id) OVERRIDE;
  virtual void EnableContinue(bool enabled) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void GetAdditionalParameters(base::DictionaryValue* dict) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // InputMethodManager::Observer implementation:
  virtual void InputMethodChanged(input_method::InputMethodManager* manager,
                                  bool show_message) OVERRIDE;

  // Reloads localized contents.
  void ReloadLocalizedContent();

 private:
  // Handles moving off the screen.
  void HandleOnExit();

  // Handles change of the language.
  void HandleOnLanguageChanged(const std::string& locale);

  // Async callback after ReloadResourceBundle(locale) completed.
  static void OnLanguageChangedCallback(
      scoped_ptr<NetworkScreenHandlerOnLanguageChangedCallbackData> context,
      const std::string& requested_locale,
      const std::string& loaded_locale,
      const bool success);

  // Handles change of the input method.
  void HandleOnInputMethodChanged(const std::string& id);

  // Handles change of the time zone
  void HandleOnTimezoneChanged(const std::string& timezone);

  // Callback when the system timezone settings is changed.
  void OnSystemTimezoneChanged();

  // Returns available timezones. Caller gets the ownership.
  static base::ListValue* GetTimezoneList();

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

  base::WeakPtrFactory<NetworkScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
