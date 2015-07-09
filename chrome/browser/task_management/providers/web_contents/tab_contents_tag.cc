// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/tab_contents_tag.h"

namespace task_management {

TabContentsTask* TabContentsTag::CreateTask() const {
  return new TabContentsTask(web_contents());
}

TabContentsTag::TabContentsTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

TabContentsTag::~TabContentsTag() {
}

}  // namespace task_management
