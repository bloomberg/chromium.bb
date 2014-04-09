// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/network_screen_actor.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "ui/gfx/point.h"

class PrefRegistrySimple;

namespace chromeos {

class CoreOobeActor;
class IdleDetector;

struct NetworkScreenHandlerOnLanguageChangedCallbackData;

// WebUI implementation of NetworkScreenActor. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class NetworkScreenHandler : public NetworkScreenActor,
                             public BaseScreenHandler,
                             public ComponentExtensionIMEManager::Observer {
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

  // ComponentExtensionIMEManager::Observer implementation:
  virtual void OnImeComponentExtensionInitialized() OVERRIDE;

  // Registers the preference for derelict state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

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

  // Demo mode detection methods.
  void StartIdleDetection();
  void StartOobeTimer();
  void OnIdle();
  void OnOobeTimerUpdate();
  void SetupTimeouts();
  bool IsDerelict();

  // Returns available languages. Caller gets the ownership. Note, it does
  // depend on the current locale.
  base::ListValue* GetLanguageList();

  // Returns available input methods. Caller gets the ownership. Note, it does
  // depend on the current locale.
  static base::ListValue* GetInputMethods();

  // Returns available timezones. Caller gets the ownership.
  static base::ListValue* GetTimezoneList();

  NetworkScreenActor::Delegate* screen_;
  CoreOobeActor* core_oobe_actor_;

  bool is_continue_enabled_;

  // Total time this machine has spent on OOBE.
  base::TimeDelta time_on_oobe_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Position of the network control.
  gfx::Point network_control_pos_;

  scoped_ptr<CrosSettings::ObserverSubscription> timezone_subscription_;

  scoped_ptr<IdleDetector> idle_detector_;

  base::RepeatingTimer<NetworkScreenHandler> oobe_timer_;

  // Timeout to detect if the machine is in a derelict state.
  base::TimeDelta derelict_detection_timeout_;

  // Timeout before showing our demo app if the machine is in a derelict state.
  base::TimeDelta derelict_idle_timeout_;

  // Time between updating our total time on oobe.
  base::TimeDelta oobe_timer_update_interval_;

  // True if should reinitialize language and keyboard list once the page
  // is ready.
  bool should_reinitialize_language_keyboard_list_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  base::WeakPtrFactory<NetworkScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_SCREEN_HANDLER_H_
