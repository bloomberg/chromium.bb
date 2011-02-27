// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEBUI_KEYBOARD_OVERLAY_UI_H_
#define CHROME_BROWSER_CHROMEOS_WEBUI_KEYBOARD_OVERLAY_UI_H_
#pragma once

#include <string>

#include "chrome/browser/ui/webui/html_dialog_ui.h"

class Browser;
class Profile;

class KeyboardOverlayUI : public HtmlDialogUI {
 public:
  explicit KeyboardOverlayUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardOverlayUI);
};

#endif  // CHROME_BROWSER_CHROMEOS_WEBUI_KEYBOARD_OVERLAY_UI_H_
