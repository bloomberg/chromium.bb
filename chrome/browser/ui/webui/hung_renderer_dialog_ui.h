// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_UI_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"

// The WebUI for chrome://hung-renderer
class HungRendererDialogUI : public HtmlDialogUI {
 public:
  explicit HungRendererDialogUI(TabContents* contents);
  virtual ~HungRendererDialogUI();

 protected:
  DISALLOW_COPY_AND_ASSIGN(HungRendererDialogUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HUNG_RENDERER_DIALOG_UI_H_
