// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_

#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class ListValue;
}

namespace chromeos {

class OobeUI;

// The core handler for Javascript messages related to the "oobe" view.
class CoreOobeHandler : public BaseScreenHandler,
                        public VersionInfoUpdater::Delegate,
                        public content::NotificationObserver {
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
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // VersionInfoUpdater::Delegate implementation:
  virtual void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) OVERRIDE;
  virtual void OnBootTimesLabelTextUpdated(
      const std::string& boot_times_label_text) OVERRIDE;
  virtual void OnEnterpriseInfoUpdated(
      const std::string& message_text) OVERRIDE;

  // Show or hide OOBE UI.
  void ShowOobeUI(bool show);

  bool show_oobe_ui() const {
    return show_oobe_ui_;
  }

 private:
  // Handlers for JS WebUI messages.
  void HandleEnableHighContrast(const base::ListValue* args);
  void HandleEnableScreenMagnifier(const base::ListValue* args);
  void HandleEnableSpokenFeedback(const base::ListValue* args);
  void HandleInitialized(const base::ListValue* args);
  void HandleSkipUpdateEnrollAfterEula(const base::ListValue* args);
  void HandleUpdateCurrentScreen(const base::ListValue* args);

  // Updates a11y menu state based on the current a11y features state(on/off).
  void UpdateA11yState();

  // Calls javascript to sync OOBE UI visibility with show_oobe_ui_.
  void UpdateOobeUIVisibility();

  // Updates label with specified id with specified text.
  void UpdateLabel(const std::string& id, const std::string& text);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Owner of this handler.
  OobeUI* oobe_ui_;

  // True if we should show OOBE instead of login.
  bool show_oobe_ui_;

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  Delegate* delegate_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CoreOobeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_CORE_OOBE_HANDLER_H_
