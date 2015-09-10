// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_

#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a RendererTask that represents background |WebContents|.
class BackgroundContentsTask : public RendererTask {
 public:
  BackgroundContentsTask(const base::string16& title,
                         BackgroundContents* background_contents);
  ~BackgroundContentsTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
