// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_

#include "chrome/browser/task_management/providers/web_contents/extension_task.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tag.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace task_management {

// Defines a concrete UserData type for WebContents owned by extensions.
class ExtensionTag : public WebContentsTag {
 public:
  // task_management::WebContentsTag:
  ExtensionTask* CreateTask() const override;

 private:
  friend class WebContentsTags;

  ExtensionTag(content::WebContents* web_contents,
               const extensions::ViewType view_type);
  ~ExtensionTag() override;

  // The ViewType of the extension WebContents this tag is attached to.
  const extensions::ViewType view_type_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTag);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TAG_H_
