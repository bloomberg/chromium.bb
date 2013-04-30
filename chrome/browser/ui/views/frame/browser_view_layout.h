// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include "ui/views/layout/layout_manager.h"

class BookmarkBarView;
class Browser;
class BrowserView;
class ContentsContainer;
class DownloadShelfView;
class InfoBarContainerView;
class TabContentsContainer;
class TabStrip;
class ToolbarView;
class WebContentsModalDialogHost;

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
  // The vertical overlap between the TabStrip and the Toolbar.
  static const int kToolbarTabStripVerticalOverlap;

  BrowserViewLayout();
  virtual ~BrowserViewLayout();

  // Sets all the views to be managed. Tests may inject stubs or NULL.
  void Init(Browser* browser,
            BrowserView* browser_view,
            InfoBarContainerView* infobar_container,
            views::SingleSplitView* contents_split,
            ContentsContainer* contents_container);

  // Sets or updates views that are not available when |this| is initialized.
  void set_bookmark_bar(BookmarkBarView* bookmark_bar) {
    bookmark_bar_ = bookmark_bar;
  }
  void set_download_shelf(DownloadShelfView* download_shelf) {
    download_shelf_ = download_shelf;
  }

  WebContentsModalDialogHost* GetWebContentsModalDialogHost();

  // Returns the minimum size of the browser view.
  gfx::Size GetMinimumSize();

  // Returns the bounding box, in widget coordinates,  for the find bar.
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
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, BrowserViewLayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, Layout);
  class WebContentsModalDialogHostViews;

  enum InstantUIState {
    // No instant suggestions are being shown.
    kInstantUINone,
    // Instant suggestions are displayed in a overlay overlapping the tab
    // contents.
    kInstantUIOverlay,
    // Instant suggestions are displayed in the main tab contents.
    kInstantUIFullPageResults,
  };

  Browser* browser() { return browser_; }

  // Layout the tab strip region, returns the coordinate of the bottom of the
  // TabStrip, for laying out subsequent controls.
  int LayoutTabStripRegion();

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top);
  int LayoutBookmarkBar(int top);
  int LayoutInfoBar(int top);

  // Layout the |contents_split_| view between the coordinates |top| and
  // |bottom|. See browser_view.h for details of the relationship between
  // |contents_split_| and other views.
  void LayoutContentsSplitView(int top, int bottom);

  // Returns the vertical offset for the web contents to account for a
  // detached bookmarks bar.
  int GetContentsOffsetForBookmarkBar();

  // Returns the top margin to adjust the contents_container_ by. This is used
  // to make the bookmark bar and contents_container_ overlap so that the
  // preview contents hides the bookmark bar.
  int GetTopMarginForActiveContent();

  // Returns the top margin by which to adjust the instant overlay web
  // contents. Used to make instant extended suggestions align with the
  // omnibox in immersive fullscreen.
  int GetTopMarginForOverlayContent();

  // Returns the top margin for the active or overlay web view in
  // |contents_container_| for an immersive fullscreen reveal while instant
  // extended is providing suggestions.
  int GetTopMarginForImmersiveInstant();

  // Returns the state of instant extended suggestions.
  InstantUIState GetInstantUIState();

  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);

  // Returns true if an infobar is showing.
  bool InfobarVisible() const;

  // The browser from the owning BrowserView.
  Browser* browser_;

  // The owning BrowserView. May be NULL in tests.
  BrowserView* browser_view_;

  // Child views that the layout manager manages.
  BookmarkBarView* bookmark_bar_;
  InfoBarContainerView* infobar_container_;
  views::SingleSplitView* contents_split_;
  ContentsContainer* contents_container_;
  DownloadShelfView* download_shelf_;

  // The bounds within which the vertically-stacked contents of the BrowserView
  // should be laid out within. This is just the local bounds of the
  // BrowserView.
  // TODO(jamescook): Remove this and just use browser_view_->GetLocalBounds().
  gfx::Rect vertical_layout_rect_;

  // The host for use in positioning the web contents modal dialog.
  scoped_ptr<WebContentsModalDialogHostViews> dialog_host_;

  // The distance the web contents modal dialog is from the top of the window,
  // in pixels.
  int web_contents_modal_dialog_top_y_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_H_
