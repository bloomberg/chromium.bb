// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace settings {

// Incompatible Applications settings page UI handler.
class IncompatibleApplicationsHandler : public SettingsPageUIHandler {
 public:
  IncompatibleApplicationsHandler();
  ~IncompatibleApplicationsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Sends the list of incompatible applications to the caller via a promise.
  void HandleRequestIncompatibleApplicationsList(const base::ListValue* args);

  // Initiates the uninstallation of the program passed using |args|.
  void HandleStartProgramUninstallation(const base::ListValue* args);

  void HandleGetSubtitlePluralString(const base::ListValue* args);
  void HandleGetSubtitleNoAdminRightsPluralString(const base::ListValue* args);
  void HandleGetListTitlePluralString(const base::ListValue* args);
  void GetPluralString(int id, const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(IncompatibleApplicationsHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_
