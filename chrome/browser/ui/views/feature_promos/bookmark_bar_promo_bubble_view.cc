// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/bookmark_bar_promo_bubble_view.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_node.h"

struct BookmarkBarPromoBubbleView::BubbleImpl : public FeaturePromoBubbleView {
  // Anchors the BookmarkBarPromoBubbleView to |anchor_view|.
  // The bubble widget and promo are owned by their native widget.
  explicit BubbleImpl(views::View* anchor_view)
      : FeaturePromoBubbleView(anchor_view,
                               views::BubbleBorder::TOP_LEFT,
                               IDS_BOOKMARK_BAR_PROMO_DEFAULT,
                               ActivationAction::DO_NOT_ACTIVATE) {}

  ~BubbleImpl() override = default;
};

BookmarkBarPromoBubbleView::BookmarkBarPromoBubbleView() = default;

void BookmarkBarPromoBubbleView::ShowForNode(
    const bookmarks::BookmarkNode* node) {
  views::LabelButton* anchor_view =
      BrowserView::GetBrowserViewForBrowser(
          BrowserList::GetInstance()->GetLastActive())
          ->bookmark_bar()
          ->GetBookmarkButtonForNode(node);
  new BookmarkBarPromoBubbleView::BubbleImpl(anchor_view);
}
