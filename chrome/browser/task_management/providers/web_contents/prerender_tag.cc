// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/prerender_tag.h"

namespace task_management {

PrerenderTask* PrerenderTag::CreateTask() const {
  return new PrerenderTask(web_contents());
}

PrerenderTag::PrerenderTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

PrerenderTag::~PrerenderTag() {
}

}  // namespace task_management
