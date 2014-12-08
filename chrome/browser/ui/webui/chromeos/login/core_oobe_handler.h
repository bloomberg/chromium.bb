// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include <string>

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_mode_detector.h"

namespace base {
class ListValue;
}

namespace gfx {
class Rect;
}

namespace chromeos {

class HelpAppLauncher;
class OobeUI;

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseScreenHandler,
                        public VersionInfoUpdater::Delegate,
                        public CoreOobeActor {
 public:
  class Delegate {
   public:
    // Called when current screen is changed.
    virtual void OnCurrentScreenChanged(const std::string& screen) = 0;
  };

  explicit CoreOobeHandler(OobeUI* oobe_ui);
  virtual ~CoreOobeHandler();

  void SetDelegate(Delegate* delegate);

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) override;
  virtual void Initialize() override;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() override;

  // VersionInfoUpdater::Delegate implementation:
  virtual void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  virtual void OnEnterpriseInfoUpdated(
      const std::string& message_text) override;

  // Show or hide OOBE UI.
  void ShowOobeUI(bool show);

  bool show_oobe_ui() const {
    return show_oobe_ui_;
  }

 private:
  // CoreOobeActor implementation:
  virtual void ShowSignInError(
      int login_attempts,
      const std::string& error_text,
      const std::string& help_link_text,
      HelpAppLauncher::HelpTopic help_topic_id) override;
  virtual void ShowTpmError() override;
  virtual void ShowSignInUI(const std::string& email) override;
  virtual void ResetSignInUI(bool force_online) override;
  virtual void ClearUserPodPassword() override;
  virtual void RefocusCurrentPod() override;
  virtual void ShowPasswordChangedScreen(bool show_password_error) override;
  virtual void SetUsageStats(bool checked) override;
  virtual void SetOemEulaUrl(const std::string& oem_eula_url) override;
  virtual void SetTpmPassword(const std::string& tmp_password) override;
  virtual void ClearErrors() override;
  virtual void ReloadContent(const base::DictionaryValue& dictionary) override;
  virtual void ShowControlBar(bool show) override;
  virtual void SetKeyboardState(bool shown, const gfx::Rect& bounds) override;
  virtual void SetClientAreaSize(int width, int height) override;
  virtual void ShowDeviceResetScreen() override;
  virtual void ShowEnableDebuggingScreen() override;

  virtual void InitDemoModeDetection() override;
  virtual void StopDemoModeDetection() override;

  // Handlers for JS WebUI messages.
  void HandleEnableLargeCursor(bool enabled);
  void HandleEnableHighContrast(bool enabled);
  void HandleEnableVirtualKeyboard(bool enabled);
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
  void HandleSwitchToNewOobe();

  // Updates a11y menu state based on the current a11y features state(on/off).
  void UpdateA11yState();

  // Calls javascript to sync OOBE UI visibility with show_oobe_ui_.
  void UpdateOobeUIVisibility();

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // Updates the device requisition string on the UI side.
  void UpdateDeviceRequisition();

  // Updates virtual keyboard state.
  void UpdateKeyboardState();

  // Updates client area size based on the primary screen size.
  void UpdateClientAreaSize();

  // Notification of a change in the accessibility settings.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // Owner of this handler.
  OobeUI* oobe_ui_;

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  // Help application used for help dialogs.
  scoped_refptr<HelpAppLauncher> help_app_;

  Delegate* delegate_;

  scoped_ptr<AccessibilityStatusSubscription> accessibility_subscription_;

  DemoModeDetector demo_mode_detector_;

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
