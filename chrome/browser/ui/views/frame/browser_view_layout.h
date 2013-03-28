// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"
#include "ui/views/layout/layout_manager.h"

class BookmarkBarView;
class Browser;
class BrowserView;
class ContentsContainer;
class DownloadShelfView;
class TabContentsContainer;
class TabStrip;
class ToolbarView;

namespace gfx {
class Point;
class Size;
}

namespace views {
class SingleSplitView;
}

// The layout manager used in chrome browser.
class BrowserViewLayout : public views::LayoutManager {
 public:
  BrowserViewLayout();
  virtual ~BrowserViewLayout();

  bool GetConstrainedWindowTopY(int* top_y);

  // Returns the minimum size of the browser view.
  gfx::Size GetMinimumSize();

  // Returns the bounding box for the find bar.
  gfx::Rect GetFindBarBoundingBox() const;

  // Returns true if the specified point(BrowserView coordinates) is in
  // in the window caption area of the browser window.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Tests to see if the specified |point| (in nonclient view's coordinates)
  // is within the views managed by the laymanager. Returns one of
  // HitTestCompat enum defined in ui/base/hit_test.h.
  // See also ClientView::NonClientHitTest.
  int NonClientHitTest(const gfx::Point& point);

  // views::LayoutManager overrides:
  virtual void Installed(views::View* host) OVERRIDE;
  virtual void Uninstalled(views::View* host) OVERRIDE;
  virtual void ViewAdded(views::View* host, views::View* view) OVERRIDE;
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;

 protected:
  Browser* browser();
  const Browser* browser() const;

  // Layout the tab strip region, returns the coordinate of the bottom of the
  // TabStrip, for laying out subsequent controls.
  int LayoutTabStripRegion();

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);

  // Layout the WebContents container, between the coordinates |top| and
  // |bottom|.
  void LayoutTabContents(int top, int bottom);

  // Returns the top margin to adjust the contents_container_ by. This is used
  // to make the bookmark bar and contents_container_ overlap so that the
  // preview contents hides the bookmark bar.
  int GetTopMarginForActiveContent();

  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);

  // Returns true if an infobar is showing.
  bool InfobarVisible() const;

  // See description above vertical_layout_rect_ for details.
  void set_vertical_layout_rect(const gfx::Rect& bounds) {
    vertical_layout_rect_ = bounds;
  }
  const gfx::Rect& vertical_layout_rect() const {
    return vertical_layout_rect_;
  }

  // Child views that the layout manager manages.
  views::SingleSplitView* contents_split_;
  ContentsContainer* contents_container_;
  DownloadShelfView* download_shelf_;

  BrowserView* browser_view_;

  // The bounds within which the vertically-stacked contents of the BrowserView
  // should be laid out within. This is just the local bounds of the
  // BrowserView.
  gfx::Rect vertical_layout_rect_;

  // The distance the FindBar is from the top of the window, in pixels.
  int find_bar_y_;

  // The distance the constrained window is from the top of the window,
  // in pixels.
  int constrained_window_top_y_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
