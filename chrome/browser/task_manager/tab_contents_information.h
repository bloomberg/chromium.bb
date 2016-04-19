// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_TAB_CONTENTS_INFORMATION_H_
#define CHROME_BROWSER_TASK_MANAGER_TAB_CONTENTS_INFORMATION_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/web_contents_information.h"

namespace task_manager {

// WebContentsInformation for WebContentses that are part of the tab strip
// model, devtools, and/or the PrerenderManager. These three cases are combined
// into one category because, for example, a prerendered WebContents can become
// part of the tab strip model over its lifetime.
class TabContentsInformation
    : public NotificationObservingWebContentsInformation {
 public:
  TabContentsInformation();
  ~TabContentsInformation() override;

  // WebContentsInformation implementation.
  bool CheckOwnership(content::WebContents* web_contents) override;
  void GetAll(const NewWebContentsCallback& callback) override;
  std::unique_ptr<RendererResource> MakeResource(
      content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabContentsInformation);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_TAB_CONTENTS_INFORMATION_H_
