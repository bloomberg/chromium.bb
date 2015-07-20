// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/panel_tag.h"

#include "chrome/browser/ui/panels/panel.h"

namespace task_management {

PanelTask* PanelTag::CreateTask() const {
  return new PanelTask(panel_, web_contents());
}

PanelTag::PanelTag(content::WebContents* web_contents, Panel* panel)
    : WebContentsTag(web_contents),
      panel_(panel) {
}

PanelTag::~PanelTag() {
}

}  // namespace task_management
