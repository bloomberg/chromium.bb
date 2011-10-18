// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCK_SCREEN_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCK_SCREEN_UI_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

namespace chromeos {

// The WebUI for Screen Locker.
class LockScreenUI : public ChromeWebUI {
 public:
  explicit LockScreenUI(TabContents* contents);
  virtual ~LockScreenUI();

 private:
  // Send an unlock request to the active screen locker.
  void UnlockScreenRequest(const base::ListValue* args);

  // Signout the active user.
  void SignoutRequest(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LockScreenUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_LOCK_SCREEN_UI_H_
