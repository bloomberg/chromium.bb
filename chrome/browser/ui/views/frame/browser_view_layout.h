// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"
#include "ui/views/layout/layout_manager.h"

class AbstractTabStripView;
class BookmarkBarView;
class Browser;
class BrowserView;
class ContentsContainer;
class DownloadShelfView;
class TabContentsContainer;
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

  // Returns the minimum size of the browser view.
  virtual gfx::Size GetMinimumSize();

  // Returns the bounding box for the find bar.
  virtual gfx::Rect GetFindBarBoundingBox() const;

  // Returns true if the specified point(BrowserView coordinates) is in
  // in the window caption area of the browser window.
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);

  // Tests to see if the specified |point| (in nonclient view's coordinates)
  // is within the views managed by the laymanager. Returns one of
  // HitTestCompat enum defined in ui/base/hit_test.h.
  // See also ClientView::NonClientHitTest.
  virtual int NonClientHitTest(const gfx::Point& point);

  // views::LayoutManager overrides:
  virtual void Installed(views::View* host) OVERRIDE;
  virtual void Uninstalled(views::View* host) OVERRIDE;
  virtual void ViewAdded(views::View* host, views::View* view) OVERRIDE;
  virtual void ViewRemoved(views::View* host, views::View* view) OVERRIDE;
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;

 protected:
  Browser* browser();
  const Browser* browser() const;

  // Layout the tab strip region, returns the coordinate of the bottom of the
  // TabStrip, for laying out subsequent controls.
  virtual int LayoutTabStripRegion();

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  virtual int LayoutToolbar(int top);
  virtual int LayoutBookmarkAndInfoBars(int top);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);

  // Updates |source|'s reserved contents rect by mapping BrowserView's
  // |browser_reserved_rect| into |future_source_bounds| taking into
  // account |source|'s |future_parent_offset| (offset is relative to
  // browser_view_).
  void UpdateReservedContentsRect(const gfx::Rect& browser_reserved_rect,
                                  TabContentsContainer* source,
                                  const gfx::Rect& future_source_bounds,
                                  const gfx::Point& future_parent_offset);

  // Layout the TabContents container, between the coordinates |top| and
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
  AbstractTabStripView* tabstrip_;
  ToolbarView* toolbar_;
  views::SingleSplitView* contents_split_;
  ContentsContainer* contents_container_;
  views::View* infobar_container_;
  DownloadShelfView* download_shelf_;
  BookmarkBarView* active_bookmark_bar_;

  BrowserView* browser_view_;

  // The bounds within which the vertically-stacked contents of the BrowserView
  // should be laid out within. This is just the local bounds of the
  // BrowserView.
  gfx::Rect vertical_layout_rect_;

  // The distance the FindBar is from the top of the window, in pixels.
  int find_bar_y_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
