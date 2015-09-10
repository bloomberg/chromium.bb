// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRINTING_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRINTING_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a task manager representation for WebContents that are created for
// print previews and background printing.
class PrintingTask : public RendererTask {
 public:
  explicit PrintingTask(content::WebContents* web_contents);
  ~PrintingTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintingTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRINTING_TASK_H_
