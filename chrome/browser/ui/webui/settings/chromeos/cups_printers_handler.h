// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}  // namespace base

class Profile;

namespace chromeos {
namespace settings {

// Chrome OS CUPS printing settings page UI handler.
class CupsPrintersHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit CupsPrintersHandler(content::WebUI* webui);
  ~CupsPrintersHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override{};
  void OnJavascriptDisallowed() override{};

 private:
  // Gets all CUPS printers and return it to WebUI.
  void HandleGetCupsPrintersList(const base::ListValue* args);

  void HandleUpdateCupsPrinter(const base::ListValue* args);
  void HandleRemoveCupsPrinter(const base::ListValue* args);

  Profile* profile_;
  base::WeakPtrFactory<CupsPrintersHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintersHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
