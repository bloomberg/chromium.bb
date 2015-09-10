// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a task manager representation of WebContents owned by the
// PrerenderManager.
class PrerenderTask : public RendererTask {
 public:
  explicit PrerenderTask(content::WebContents* web_contents);
  ~PrerenderTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrerenderTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_
