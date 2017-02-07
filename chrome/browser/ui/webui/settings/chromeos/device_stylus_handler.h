// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STYLUS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STYLUS_HANDLER_H_

#include <set>

#include "base/macros.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace chromeos {
namespace settings {

class StylusHandler : public ::settings::SettingsPageUIHandler,
                      public chromeos::NoteTakingHelper::Observer {
 public:
  StylusHandler();
  ~StylusHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  // chromeos::NoteTakingHelper::Observer implementation.
  void OnAvailableNoteTakingAppsUpdated() override;

 private:
  void UpdateNoteTakingApps();
  void RequestApps(const base::ListValue* unused_args);
  void SetPreferredNoteTakingApp(const base::ListValue* args);

  // IDs of available note-taking apps.
  std::set<std::string> note_taking_app_ids_;

  DISALLOW_COPY_AND_ASSIGN(StylusHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STYLUS_HANDLER_H_
