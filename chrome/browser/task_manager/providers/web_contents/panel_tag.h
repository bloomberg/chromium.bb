// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PANEL_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PANEL_TAG_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/web_contents/panel_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by the PanelManager.
class PanelTag : public WebContentsTag {
 public:
  // task_manager::WebContentsTag:
  PanelTask* CreateTask() const override;

 private:
  friend class WebContentsTags;

  PanelTag(content::WebContents* web_contents, Panel* panel);
  ~PanelTag() override;

  Panel* panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PANEL_TAG_H_
