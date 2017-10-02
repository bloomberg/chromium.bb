// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class ScopedVisibilityTracker;

// This class tracks new popups, and is used to log metrics on the visibility
// time of the first document in the popup.
// TODO(csharrison): Consider adding more metrics like total visibility for the
// lifetime of the WebContents.
class PopupTracker : public content::WebContentsObserver,
                     public content::WebContentsUserData<PopupTracker> {
 public:
  static void CreateForWebContents(content::WebContents* contents,
                                   content::WebContents* opener);
  ~PopupTracker() override;

 private:
  friend class content::WebContentsUserData<PopupTracker>;

  PopupTracker(content::WebContents* contents, content::WebContents* opener);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasShown() override;
  void WasHidden() override;

  // The |first_load_visibility_tracker_| tracks the time this WebContents is in
  // the foreground for the duration of the first page load.
  std::unique_ptr<ScopedVisibilityTracker> first_load_visibility_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PopupTracker);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_
