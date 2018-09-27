// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_BAR_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_BAR_PROMO_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/welcome/nux/show_promo_delegate.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// The BookmarkBarPromoBubbleView is a bubble anchored to a bookmark on the
// bookmark bar. It is to draw attention to a bookmark to help new users
// understand the actions taken during NUX experiments.
//
// A concrete instance is created and passed to the NUX experiments in
// //components so that the experiments can call the delegate with the
// relevant bookmark node.
class BookmarkBarPromoBubbleView : public ShowPromoDelegate {
 public:
  explicit BookmarkBarPromoBubbleView(int string_specifier);
  ~BookmarkBarPromoBubbleView() override = default;

  // ShowPromoDelegate:
  // Shows a promo bubble for a node in the bookmark bar. The view of the
  // bubble is owned by the view for the node.
  void ShowForNode(const bookmarks::BookmarkNode* node) override;

 private:
  struct BubbleImpl;
  class BookmarkBarViewObserverImpl;

  // The string that will be shown on this bubble.
  int string_specifier;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarPromoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_BAR_PROMO_BUBBLE_VIEW_H_
