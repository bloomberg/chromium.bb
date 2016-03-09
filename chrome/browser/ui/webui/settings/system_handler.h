// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

namespace base {
class ListValue;
}

namespace settings {

class SystemHandler : public SettingsPageUIHandler {
 public:
  SystemHandler();
  ~SystemHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

 private:
  // Handler for the "changeProxySettings" message. No args.
  void HandleChangeProxySettings(const base::ListValue* /*args*/);

  DISALLOW_COPY_AND_ASSIGN(SystemHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SYSTEM_HANDLER_H_
