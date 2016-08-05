// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {
namespace settings {

// Chrome OS Internet settings page UI handler.
class InternetHandler : public ::settings::SettingsPageUIHandler {
 public:
  InternetHandler();
  ~InternetHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Settings JS handlers.
  void AddNetwork(const base::ListValue* args);
  void ConfigureNetwork(const base::ListValue* args);

  gfx::NativeWindow GetNativeWindow() const;

  DISALLOW_COPY_AND_ASSIGN(InternetHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_INTERNET_HANDLER_H_
