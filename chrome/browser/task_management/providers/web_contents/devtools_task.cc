// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/devtools_task.h"

namespace task_management {

DevToolsTask::DevToolsTask(content::WebContents* web_contents)
    : RendererTask(RendererTask::GetTitleFromWebContents(web_contents),
                   RendererTask::GetFaviconFromWebContents(web_contents),
                   web_contents) {
}

DevToolsTask::~DevToolsTask() {
}

// task_management::RendererTask:
void DevToolsTask::OnTitleChanged(content::NavigationEntry* entry) {
  // In the case of a devtools tab, this won't happen so we ignore it.
  // TODO(afakhry): Confirm that it doesn't happen in practice.
  NOTREACHED();
}

void DevToolsTask::OnFaviconChanged() {
  // In the case of a devtools tab, this won't happen so we ignore it.
  // TODO(afakhry): Confirm that it doesn't happen in practice.
  NOTREACHED();
}

}  // namespace task_management
