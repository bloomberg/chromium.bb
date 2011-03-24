// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SIM_UNLOCK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SIM_UNLOCK_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

namespace chromeos {

// A custom WebUI that defines datasource for SIM unlock dialog that is used
// in Chrome OS for specific tasks:
// - Unlock SIM card (enter PIN/PUK codes).
// - Display "SIM card is blocked" message when there're no PUK tries left.
class SimUnlockUI : public WebUI {
 public:
  explicit SimUnlockUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(SimUnlockUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SIM_UNLOCK_UI_H_
