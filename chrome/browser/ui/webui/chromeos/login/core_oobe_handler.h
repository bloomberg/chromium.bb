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
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // VersionInfoUpdater::Delegate implementation:
  virtual void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) OVERRIDE;
  virtual void OnEnterpriseInfoUpdated(
      const std::string& message_text) OVERRIDE;

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
      HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;
  virtual void ShowTpmError() OVERRIDE;
  virtual void ShowSignInUI(const std::string& email) OVERRIDE;
  virtual void ResetSignInUI(bool force_online) OVERRIDE;
  virtual void ClearUserPodPassword() OVERRIDE;
  virtual void RefocusCurrentPod() OVERRIDE;
  virtual void ShowPasswordChangedScreen(bool show_password_error) OVERRIDE;
  virtual void SetUsageStats(bool checked) OVERRIDE;
  virtual void SetOemEulaUrl(const std::string& oem_eula_url) OVERRIDE;
  virtual void SetTpmPassword(const std::string& tmp_password) OVERRIDE;
  virtual void ClearErrors() OVERRIDE;
  virtual void ReloadContent(const base::DictionaryValue& dictionary) OVERRIDE;
  virtual void ShowControlBar(bool show) OVERRIDE;
  virtual void SetKeyboardState(bool shown, const gfx::Rect& bounds) OVERRIDE;
  virtual void SetClientAreaSize(int width, int height) OVERRIDE;
  virtual void ShowDeviceResetScreen() OVERRIDE;
  virtual void InitDemoModeDetection() OVERRIDE;
  virtual void StopDemoModeDetection() OVERRIDE;

  // Handlers for JS WebUI messages.
  void HandleEnableLargeCursor(bool enabled);
  void HandleEnableHighContrast(bool enabled);
  void HandleEnableVirtualKeyboard(bool enabled);
  void HandleEnableScreenMagnifier(bool enabled);
  void HandleEnableSpokenFeedback();
  void HandleInitialized();
  void HandleSkipUpdateEnrollAfterEula();
  void HandleUpdateCurrentScreen(const std::string& screen);
  void HandleSetDeviceRequisition(const std::string& requisition);
  void HandleScreenAssetsLoaded(const std::string& screen_async_load_id);
  void HandleSkipToLoginForTesting(const base::ListValue* args);
  void HandleLaunchHelpApp(double help_topic_id);
  void HandleToggleResetScreen();
  void HandleHeaderBarVisible();

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
