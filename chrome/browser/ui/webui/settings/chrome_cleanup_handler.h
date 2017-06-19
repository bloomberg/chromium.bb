// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROME_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROME_CLEANUP_HANDLER_H_

#include <set>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace settings {

// Chrome Cleanup settings page UI handler.
class ChromeCleanupHandler
    : public SettingsPageUIHandler,
      public safe_browsing::ChromeCleanerController::Observer {
 public:
  explicit ChromeCleanupHandler(Profile* profile);
  ~ChromeCleanupHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ChromeCleanerController::Observer implementation.
  void OnIdle(
      safe_browsing::ChromeCleanerController::IdleReason idle_reason) override;
  void OnScanning() override;
  void OnInfected(const std::set<base::FilePath>& files) override;
  void OnCleaning(const std::set<base::FilePath>& files) override;
  void OnRebootRequired() override;

 private:
  // Callback for the "dismissCleanupPage" message to hide the Cleanup page
  // from the settings.
  void HandleDismiss(const base::ListValue* args);

  // Callback for the "registerChromeCleanerObserver" message. This registers
  // this object as an observer of the Chrome Cleanup global state and
  // and retrieves the current cleanup state.
  void HandleRegisterChromeCleanerObserver(const base::ListValue* args);

  // Callback for the "restartComputer" message to finalize the cleanup with a
  // system restart.
  void HandleRestartComputer(const base::ListValue* args);

  // Callback for the "startCleanup" message to start removing unwanted
  // software from the user's computer.
  void HandleStartCleanup(const base::ListValue* args);

  // Raw pointer to a singleton. Must outlive this object.
  safe_browsing::ChromeCleanerController* controller_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanupHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROME_CLEANUP_HANDLER_H_
