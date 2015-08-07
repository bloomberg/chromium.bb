// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"
#include "extensions/common/view_type.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace task_management {

// Defines a task manager representation for extensions.
class ExtensionTask : public RendererTask {
 public:
  ExtensionTask(content::WebContents* web_contents,
                const extensions::Extension* extension,
                extensions::ViewType view_type);
  ~ExtensionTask() override;

  // task_management::RendererTask:
  void OnTitleChanged(content::NavigationEntry* entry) override;
  void OnFaviconChanged() override;
  Type GetType() const override;

 private:
  // If |extension| is nullptr, this method will get the title from
  // the |web_contents|.
  base::string16 GetExtensionTitle(
      content::WebContents* web_contents,
      const extensions::Extension* extension,
      extensions::ViewType view_type) const;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_EXTENSION_TASK_H_
