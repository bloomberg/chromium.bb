// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_TAB_CONTENTS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_TAB_CONTENTS_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a task manager representation of WebContents owned by the
// TabStripModel.
class TabContentsTask : public RendererTask {
 public:
  explicit TabContentsTask(content::WebContents* web_contents);
  ~TabContentsTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  Type GetType() const override;

 private:
  base::string16 GetCurrentTitle() const;

  DISALLOW_COPY_AND_ASSIGN(TabContentsTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_TAB_CONTENTS_TASK_H_
