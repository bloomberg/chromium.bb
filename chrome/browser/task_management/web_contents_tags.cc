// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/web_contents_tags.h"

#include "chrome/browser/task_management/providers/web_contents/background_contents_tag.h"
#include "chrome/browser/task_management/providers/web_contents/devtools_tag.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "content/public/browser/web_contents.h"

namespace task_management {

namespace {

// Adds the |tag| to |contents|. It also adds the |tag| to the
// |WebContentsTagsManager|.
// Note: This will fail if |contents| is already tagged by |tag|.
void TagWebContents(content::WebContents* contents,
                    WebContentsTag* tag,
                    void* tag_key) {
  DCHECK(contents);
  DCHECK(tag);
  CHECK(WebContentsTag::FromWebContents(contents) == nullptr);
  contents->SetUserData(tag_key, tag);
  WebContentsTagsManager::GetInstance()->AddTag(tag);
}

}  // namespace

// static
void WebContentsTags::CreateForBackgroundContents(
    content::WebContents* web_contents,
    BackgroundContents* background_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(
        web_contents,
        new BackgroundContentsTag(web_contents, background_contents),
        WebContentsTag::kTagKey);
  }
}

// static
void WebContentsTags::CreateForDevToolsContents(
    content::WebContents* web_contents) {
  if (!WebContentsTag::FromWebContents(web_contents)) {
    TagWebContents(web_contents,
                   new DevToolsTag(web_contents),
                   WebContentsTag::kTagKey);
  }
}

}  // namespace task_management

