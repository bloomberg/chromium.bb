// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/gfx/size.h"
#include "ui/views/view.h"

class GURL;
class PageActionImageView;

namespace content {
class WebContents;
}

// A container for the PageActionImageView plus its badge.
class PageActionWithBadgeView
    : public views::View,
      public TouchableLocationBarView {
 public:
  explicit PageActionWithBadgeView(PageActionImageView* image_view);

  PageActionImageView* image_view() { return image_view_; }

  // View overrides:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // TouchableLocationBarView.
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

  void UpdateVisibility(content::WebContents* contents, const GURL& url);

 private:
  virtual void Layout() OVERRIDE;

  // The button this view contains.
  PageActionImageView* image_view_;

  DISALLOW_COPY_AND_ASSIGN(PageActionWithBadgeView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_ACTION_WITH_BADGE_VIEW_H_
