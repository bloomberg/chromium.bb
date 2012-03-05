// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_UI_H_
#pragma once

#include "content/public/browser/web_ui_controller.h"

class TaskManagerUI : public content::WebUIController {
 public:
  explicit TaskManagerUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TASK_MANAGER_TASK_MANAGER_UI_H_
