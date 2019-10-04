// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"
#include "content/public/browser/web_ui_controller.h"

class Browser;

// The WebUI version of the tab strip in the browser. It is currently only
// supported on ChromeOS in tablet mode.
class TabStripUI : public content::WebUIController {
 public:
  explicit TabStripUI(content::WebUI* web_ui);
  ~TabStripUI() override;

  // Initialize TabStripUI with the Browser it is running in. Must be called
  // exactly once. The WebUI won't work until this is called.
  void Initialize(Browser* browser);

 private:
  void HandleThumbnailUpdate(int extension_tab_id, gfx::ImageSkia image);

  DISALLOW_COPY_AND_ASSIGN(TabStripUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
