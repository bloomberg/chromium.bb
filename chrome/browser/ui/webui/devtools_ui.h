// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class DevToolsUI : public WebUI {
 public:
  static void RegisterDevToolsDataSource();

  explicit DevToolsUI(TabContents* contents);

  // WebUI
  virtual void RenderViewCreated(RenderViewHost* render_view_host);

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_UI_H_
