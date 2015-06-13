// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/providers/web_contents/web_contents_tag.h"

#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "content/public/browser/web_contents.h"

namespace task_management {

// static
const WebContentsTag* WebContentsTag::FromWebContents(
    const content::WebContents* contents) {
  DCHECK(contents);
  return static_cast<const WebContentsTag*>(contents->GetUserData(kTagKey));
}

WebContentsTag::WebContentsTag(content::WebContents* contents)
    : web_contents_(contents) {
  DCHECK(contents);
  // You can't tag the |contents| here. The object creation is not complete yet.
  // This will be done in the factory methods inside
  // |task_management::WebContentsTags|.
}

WebContentsTag::~WebContentsTag() {
  WebContentsTagsManager::GetInstance()->RemoveTag(this);
}

// static
void* WebContentsTag::kTagKey = &WebContentsTag::kTagKey;

}  // namespace task_management
