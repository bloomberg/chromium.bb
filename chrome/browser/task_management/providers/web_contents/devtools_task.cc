// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/devtools_task.h"

#include "content/public/browser/web_contents.h"

namespace task_management {

DevToolsTask::DevToolsTask(content::WebContents* web_contents)
    : TabContentsTask(web_contents) {
}

DevToolsTask::~DevToolsTask() {
}

}  // namespace task_management
