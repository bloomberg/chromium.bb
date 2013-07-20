// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_DELEGATE_H_

class BookmarkBubbleDelegate {
 public:
  virtual ~BookmarkBubbleDelegate() {}
  virtual void OnSignInLinkClicked() = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_BUBBLE_DELEGATE_H_
