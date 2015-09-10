// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PANEL_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PANEL_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

class Panel;

namespace task_management {

// Defines a task manager representation of WebContents owned by the
// PanelManager.
class PanelTask : public RendererTask {
 public:
  PanelTask(Panel* panel, content::WebContents* web_contents);
  ~PanelTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  Task::Type GetType() const override;

 private:
  base::string16 GetCurrentPanelTitle(Panel* panel) const;

  Panel* panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_PANEL_TASK_H_
