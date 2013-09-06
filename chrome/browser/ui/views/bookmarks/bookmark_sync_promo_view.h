// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SYNC_PROMO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SYNC_PROMO_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

class BookmarkBubbleDelegate;

// Bookmark sync promo displayed at the bottom of the bookmark bubble.
class BookmarkSyncPromoView : public views::StyledLabelListener,
                              public views::View {
 public:
  // |delegate| is not owned by BookmarkSyncPromoView.
  explicit BookmarkSyncPromoView(BookmarkBubbleDelegate* delegate);

 private:
  // views::StyledLabelListener:
  virtual void StyledLabelLinkClicked(const gfx::Range& range,
                                      int event_flags) OVERRIDE;

  // Delegate, to handle clicks on the sign in link.
  BookmarkBubbleDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSyncPromoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_SYNC_PROMO_VIEW_H_
