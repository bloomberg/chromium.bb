// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_UI_H_

#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A WebUI to host the spring charger replacement web ui.
class ChargerReplacementUI : public ui::WebDialogUI {
 public:
  explicit ChargerReplacementUI(content::WebUI* web_ui);
  virtual ~ChargerReplacementUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChargerReplacementUI);
};

}   // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHARGER_REPLACEMENT_UI_H_
