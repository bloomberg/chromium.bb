// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_OBSERVER_H_

class GURL;

class BookmarkBubbleViewObserver {
 public:
  virtual void OnBookmarkBubbleShown(const GURL& url) = 0;
  virtual void OnBookmarkBubbleHidden() = 0;

 protected:
  virtual ~BookmarkBubbleViewObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUBBLE_VIEW_OBSERVER_H_
