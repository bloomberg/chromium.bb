// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/devtools_tag.h"

namespace task_management {

// task_management::WebContentsTag:
DevToolsTask* DevToolsTag::CreateTask() const {
  return new DevToolsTask(web_contents());
}

DevToolsTag::DevToolsTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

DevToolsTag::~DevToolsTag() {
}

}  // namespace task_management
