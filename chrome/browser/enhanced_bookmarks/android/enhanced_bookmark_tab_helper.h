// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARK_TAB_HELPER_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARK_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class EnhancedBookmarkTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<EnhancedBookmarkTabHelper> {
 public:
  ~EnhancedBookmarkTabHelper() override {};

  // content::WebContentsObserver overrides.
  void DocumentOnLoadCompletedInMainFrame() override;

 private:
  explicit EnhancedBookmarkTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<EnhancedBookmarkTabHelper>;

  DISALLOW_COPY_AND_ASSIGN(EnhancedBookmarkTabHelper);
};

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_ENHANCED_BOOKMARK_TAB_HELPER_H_
