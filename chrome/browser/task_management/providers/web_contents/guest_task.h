// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_

#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace task_management {

// Defines a concrete RendererTask that represents WebContents owned by the
// GuestViewBase which represents a <*view> tag that is a browser plugin guest.
class GuestTask : public RendererTask {
 public:
  explicit GuestTask(content::WebContents* web_contents);
  ~GuestTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  Type GetType() const override;

 private:
  base::string16 GetCurrentTitle(content::WebContents* web_contents) const;

  DISALLOW_COPY_AND_ASSIGN(GuestTask);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_
