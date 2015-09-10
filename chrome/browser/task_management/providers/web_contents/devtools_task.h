// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_DEVTOOLS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_DEVTOOLS_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a task manager representation of the developer tools WebContents.
class DevToolsTask : public RendererTask {
 public:
  explicit DevToolsTask(content::WebContents* web_contents);
  ~DevToolsTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_DEVTOOLS_TASK_H_
