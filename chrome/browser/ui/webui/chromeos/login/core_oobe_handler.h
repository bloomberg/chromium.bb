// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_mode_detector.h"
#include "ui/events/event_source.h"
#include "ui/keyboard/scoped_keyboard_disabler.h"

namespace base {
class ListValue;
}

namespace ui {
class EventProcessor;
}

namespace chromeos {

class HelpAppLauncher;
class OobeUI;

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseScreenHandler,
                        public VersionInfoUpdater::Delegate,
                        public CoreOobeActor,
                        public ui::EventSource {
 public:
  class Delegate {
   public:
    // Called when current screen is changed.
    virtual void OnCurrentScreenChanged(const std::string& screen) = 0;
  };

  explicit CoreOobeHandler(OobeUI* oobe_ui);
  ~CoreOobeHandler() override;

  void SetDelegate(Delegate* delegate);

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // VersionInfoUpdater::Delegate implementation:
  void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  void OnEnterpriseInfoUpdated(const std::string& message_text,
                               const std::string& asset_id) override;

  // ui::EventSource implementation:
  ui::EventProcessor* GetEventProcessor() override;

  // Show or hide OOBE UI.
  void ShowOobeUI(bool show);

  bool show_oobe_ui() const {
    return show_oobe_ui_;
  }

  // If |reboot_on_shutdown| is true, the reboot button becomes visible
  // and the shutdown button is hidden. Vice versa if |reboot_on_shutdown| is
  // false.
  void UpdateShutdownAndRebootVisibility(bool reboot_on_shutdown);

 private:
  // Calls javascript method.
  //
  // Note that the Args template parameter pack should consist of types
  // convertible to base::Value.
  template <typename... Args>
  void ExecuteDeferredJSCall(const std::string& function_name,
                             std::unique_ptr<Args>... args);

  // Calls javascript method if the instance is already initialized, or defers
  // the call until it gets initialized.
  template <typename... Args>
  void CallJSOrDefer(const std::string& function_name, const Args&... args);

  // Executes javascript calls that were deferred while the instance was not
  // initialized yet.
  void ExecuteDeferredJSCalls();

  // CoreOobeActor implementation:
  void ShowSignInError(int login_attempts,
                       const std::string& error_text,
                       const std::string& help_link_text,
                       HelpAppLauncher::HelpTopic help_topic_id) override;
  void ShowTpmError() override;
  void ShowSignInUI(const std::string& email) override;
  void ResetSignInUI(bool force_online) override;
  void ClearUserPodPassword() override;
  void RefocusCurrentPod() override;
  void ShowPasswordChangedScreen(bool show_password_error,
                                 const std::string& email) override;
  void SetUsageStats(bool checked) override;
  void SetOemEulaUrl(const std::string& oem_eula_url) override;
  void SetTpmPassword(const std::string& tmp_password) override;
  void ClearErrors() override;
  void ReloadContent(const base::DictionaryValue& dictionary) override;
  void ShowControlBar(bool show) override;
  void ShowPinKeyboard(bool show) override;
  void SetClientAreaSize(int width, int height) override;
  void ShowDeviceResetScreen() override;
  void ShowEnableDebuggingScreen() override;

  void InitDemoModeDetection() override;
  void StopDemoModeDetection() override;
  void UpdateKeyboardState() override;

  // Handlers for JS WebUI messages.
  void HandleEnableLargeCursor(bool enabled);
  void HandleEnableHighContrast(bool enabled);
  void HandleEnableVirtualKeyboard(bool enabled);
  void HandleSetForceDisableVirtualKeyboard(bool disable);
  void HandleEnableScreenMagnifier(bool enabled);
  void HandleEnableSpokenFeedback(bool /* enabled */);
  void HandleInitialized();
  void HandleSkipUpdateEnrollAfterEula();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleSetDeviceRequisition(const std::string& requisition);
  void HandleScreenAssetsLoaded(const std::string& screen_async_load_id);
  void HandleSkipToLoginForTesting(const base::ListValue* args);
  void HandleLaunchHelpApp(double help_topic_id);
  void HandleToggleResetScreen();
  void HandleEnableDebuggingScreen();
  void HandleHeaderBarVisible();
  void HandleSetOobeBootstrappingSlave();

  // When keyboard_utils.js arrow key down event is reached, raise it
  // to tab/shift-tab event.
  void HandleRaiseTabKeyEvent(bool reverse);

  // Updates a11y menu state based on the current a11y features state(on/off).
  void UpdateA11yState();

  // Calls javascript to sync OOBE UI visibility with show_oobe_ui_.
  void UpdateOobeUIVisibility();

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // Updates the device requisition string on the UI side.
  void UpdateDeviceRequisition();

  // Updates client area size based on the primary screen size.
  void UpdateClientAreaSize();

  // Notification of a change in the accessibility settings.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Whether the instance is initialized.
  //
  // The instance becomes initialized after the corresponding message is
  // received from javascript side.
  bool is_initialized_ = false;

  // Javascript calls that have been deferred while the instance was not
  // initialized yet.
  std::vector<base::Closure> deferred_js_calls_;

  // Owner of this handler.
  OobeUI* oobe_ui_ = nullptr;

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_ = false;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  Delegate* delegate_ = nullptr;

  std::unique_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DemoModeDetector demo_mode_detector_;

  keyboard::ScopedKeyboardDisabler scoped_keyboard_disabler_;

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
