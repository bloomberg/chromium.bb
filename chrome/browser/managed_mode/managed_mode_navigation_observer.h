// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarDelegate;
class ManagedModeURLFilter;

class ManagedModeNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagedModeNavigationObserver> {
 public:
  virtual ~ManagedModeNavigationObserver();

  void WarnInfobarDismissed();

 private:
  friend class content::WebContentsUserData<ManagedModeNavigationObserver>;

  explicit ManagedModeNavigationObserver(content::WebContents* web_contents);

  // content::WebContentsObserver implementation.
  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Owned by ManagedMode (which is a singleton and outlives us).
  const ManagedModeURLFilter* url_filter_;

  // Owned by the InfoBarService, which has the same lifetime as this object.
  InfoBarDelegate* warn_infobar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagedModeNavigationObserver);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_MODE_NAVIGATION_OBSERVER_H_
