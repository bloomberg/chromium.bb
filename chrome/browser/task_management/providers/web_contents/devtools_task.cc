// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/devtools_task.h"

#include "content/public/browser/web_contents.h"

namespace task_management {

DevToolsTask::DevToolsTask(content::WebContents* web_contents)
    : RendererTask(RendererTask::GetTitleFromWebContents(web_contents),
                   RendererTask::GetFaviconFromWebContents(web_contents),
                   web_contents,
                   web_contents->GetRenderProcessHost()) {
}

DevToolsTask::~DevToolsTask() {
}

// task_management::RendererTask:
void DevToolsTask::UpdateTitle() {
  // In the case of a devtools tab, we just ignore this event.
}

void DevToolsTask::UpdateFavicon() {
  // In the case of a devtools tab, we just ignore this event.
}

}  // namespace task_management
