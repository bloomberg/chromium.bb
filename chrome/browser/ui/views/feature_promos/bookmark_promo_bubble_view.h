// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_PROMO_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"

// The BookmarkPromoBubbleView is a bubble anchored to the left of StarView
// to draw attention to StarView. It is created by the StarView when
// prompted by the feature_engagement::BookmarkTracker.
class BookmarkPromoBubbleView : public FeaturePromoBubbleView {
 public:
  // Returns a raw pointer that is owned by its native widget.
  static BookmarkPromoBubbleView* CreateOwned(views::View* anchor_view);

 private:
  // Anchors the BookmarkPromoBubbleView to |anchor_view|.
  // The bubble widget and promo are owned by their native widget.
  explicit BookmarkPromoBubbleView(views::View* anchor_view);
  ~BookmarkPromoBubbleView() override;

  // Returns the string ID to display in the promo.
  static int GetStringSpecifier();

  DISALLOW_COPY_AND_ASSIGN(BookmarkPromoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FEATURE_PROMOS_BOOKMARK_PROMO_BUBBLE_VIEW_H_
