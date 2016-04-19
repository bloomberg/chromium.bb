// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace offline_pages {

// Attaches to every WebContent shown in a tab. Waits until the WebContent is
// loaded to proper degree and then makes a snapshot of the page. Removes the
// oldest snapshot in the 'ring buffer'. As a result, there is always up to N
// snapshots of recent pages on the device.
class RecentTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RecentTabHelper> {
 public:
  ~RecentTabHelper() override;

 private:
  explicit RecentTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<RecentTabHelper>;

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelper);
};

}  // namespace offline_pages
#endif  // CHROME_BROWSER_ANDROID_OFFLINE_PAGES_RECENT_TAB_HELPER_H_
