// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace content {
class WebUI;
}

class Profile;

namespace settings {

// Chrome "Languages" settings page UI handler.
class LanguagesHandler : public SettingsPageUIHandler {
 public:
  explicit LanguagesHandler(content::WebUI* webui);
  ~LanguagesHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Changes the UI language, provided the user is allowed to do so.
  void HandleSetUILanguage(const base::ListValue* args);

  Profile* profile_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(LanguagesHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_LANGUAGES_HANDLER_H_
