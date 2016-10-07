// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace settings {

// Chrome OS date and time settings page UI handler.
class DateTimeHandler : public ::settings::SettingsPageUIHandler {
 public:
  ~DateTimeHandler() override;

  // Adds load-time values to html_source before creating the handler.
  static DateTimeHandler* Create(content::WebUIDataSource* html_source);

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  DateTimeHandler();

  DISALLOW_COPY_AND_ASSIGN(DateTimeHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DATE_TIME_HANDLER_H_
