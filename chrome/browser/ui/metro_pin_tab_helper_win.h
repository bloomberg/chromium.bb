// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image_skia.h"

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetroPinTabHelper> {
 public:
  virtual ~MetroPinTabHelper();

  bool is_pinned() const { return is_pinned_; }

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // The TaskRunner handles running tasks for this helper on the FILE thread.
  class TaskRunner;

  explicit MetroPinTabHelper(content::WebContents* tab_contents);
  friend class content::WebContentsUserData<MetroPinTabHelper>;

  // Queries the metro driver about the pinned state of the current URL.
  void UpdatePinnedStateForCurrentURL();

  void UnPinPageFromStartScreen();

  // Whether the current URL is pinned to the metro start screen.
  bool is_pinned_;

  // The task runner for the helper, which runs things for it on the FILE
  // thread.
  scoped_refptr<TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_WIN_H_
