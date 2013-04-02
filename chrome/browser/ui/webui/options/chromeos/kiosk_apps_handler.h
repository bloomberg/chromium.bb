// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_

#include <string>

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ui/webui/options/chromeos/core_chromeos_options_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

class KioskAppManager;

namespace options {

class KioskAppsHandler : public CoreChromeOSOptionsHandler,
                         public KioskAppManagerObserver {
 public:
  KioskAppsHandler();
  virtual ~KioskAppsHandler();

  // options::OptionsPageUIHandler overrides:
  virtual void RegisterMessages() OVERRIDE;
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // KioskAppPrefsObserver overrides:
  virtual void OnKioskAppDataChanged(const std::string& app_id) OVERRIDE;
  virtual void OnKioskAppDataLoadFailure(const std::string& app_id) OVERRIDE;

 private:
  // Sends all kiosk apps to webui.
  void SendKioskApps();

  // JS callbacks.
  void HandleGetKioskApps(const base::ListValue* args);
  void HandleAddKioskApp(const base::ListValue* args);
  void HandleRemoveKioskApp(const base::ListValue* args);
  void HandleEnableKioskAutoLaunch(const base::ListValue* args);
  void HandleDisableKioskAutoLaunch(const base::ListValue* args);

  KioskAppManager* kiosk_app_manager_;  // not owned.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_KIOSK_APPS_HANDLER_H_
