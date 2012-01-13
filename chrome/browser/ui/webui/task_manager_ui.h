// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

class TaskManagerUI : public WebUI, public content::WebUIController {
 public:
  explicit TaskManagerUI(content::WebContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_UI_H_
