// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_UI_H_
#pragma once

#include "chrome/browser/ui/webui/html_dialog_ui.h"

// The WebUI for chrome://input-window-dialog
class InputWindowDialogUI : public HtmlDialogUI {
 public:
  explicit InputWindowDialogUI(content::WebContents* contents);
  virtual ~InputWindowDialogUI();

 protected:
  DISALLOW_COPY_AND_ASSIGN(InputWindowDialogUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INPUT_WINDOW_DIALOG_UI_H_
