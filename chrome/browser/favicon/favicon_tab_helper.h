// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_

#include "base/macros.h"
#include "components/favicon/content/content_favicon_driver.h"

// FaviconTabHelper provides helper factory for ContentFaviconDriver.
class FaviconTabHelper : public favicon::ContentFaviconDriver {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  // TODO(sdefresne): remove this method once all clients have been ported to
  // use ContentFaviconDriver::FromWebContents() instead.
  static FaviconTabHelper* FromWebContents(content::WebContents* web_contents);

  // Returns whether the favicon should be displayed. If this returns false, no
  // space is provided for the favicon, and the favicon is never displayed.
  static bool ShouldDisplayFavicon(content::WebContents* web_contents);

 private:
  friend class FaviconTabHelperTest;

  // Creates a new FaviconTabHelper bound to |web_contents|. Initialize
  // |favicon_service_|, |bookmark_model_| and |history_service_| from the
  // corresponding parameter.
  FaviconTabHelper(content::WebContents* web_contents,
                   favicon::FaviconService* favicon_service,
                   history::HistoryService* history_service,
                   bookmarks::BookmarkModel* bookmark_model);

  DISALLOW_COPY_AND_ASSIGN(FaviconTabHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_TAB_HELPER_H_
