// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/printing_tag.h"

namespace task_management {

PrintingTask* PrintingTag::CreateTask() const {
  return new PrintingTask(web_contents());
}

PrintingTag::PrintingTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

PrintingTag::~PrintingTag() {
}

}  // namespace task_management
