// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PRINTING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PRINTING_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

// UI handler for Chrome printing setting subpage on operating systems other
// than Chrome OS.
class PrintingHandler : public SettingsPageUIHandler {
 public:
  PrintingHandler();
  ~PrintingHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleOpenSystemPrintDialog(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(PrintingHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PRINTING_HANDLER_H_
