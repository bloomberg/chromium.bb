// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_CHROME_BROWSER_VIEW_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_VIEWS_FRAME_CHROME_BROWSER_VIEW_LAYOUT_MANAGER_H_

#include "chrome/browser/views/frame/browser_view_layout_manager.h"
#include "chrome/browser/views/frame/browser_view.h"

// The layout manager used in chrome browser.
class ChromeBrowserViewLayoutManager : public BrowserViewLayoutManager {
 public:
  ChromeBrowserViewLayoutManager();
  virtual ~ChromeBrowserViewLayoutManager() {}

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from LayoutManager.
  virtual void Installed(views::View* host);
  virtual void Uninstalled(views::View* host);
  virtual void ViewAdded(views::View* host, views::View* view);
  virtual void ViewRemoved(views::View* host, views::View* view);
  virtual void Layout(views::View* host);
  virtual gfx::Size GetPreferredSize(views::View* host);

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from BrowserLayoutManager.
  virtual gfx::Size GetMinimumSize();
  virtual gfx::Rect GetFindBarBoundingBox() const;
  virtual bool IsPositionInWindowCaption(const gfx::Point& point);
  virtual int NonClientHitTest(const gfx::Point& point);

 protected:
  Browser* browser() {
    return browser_view_->browser();
  }

  // Layout the TabStrip, returns the coordinate of the bottom of the TabStrip,
  // for laying out subsequent controls.
  virtual int LayoutTabStrip();

  // Layout the following controls, starting at |top|, returns the coordinate
  // of the bottom of the control, for laying out the next control.
  int LayoutToolbar(int top);
  int LayoutBookmarkAndInfoBars(int top);
  int LayoutTopBar(int top);
  int LayoutInfoBar(int top);

  // Layout the TabContents container, between the coordinates |top| and
  // |bottom|.
  void LayoutTabContents(int top, int bottom);
  int LayoutExtensionAndDownloadShelves();

  // Layout the Download Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutDownloadShelf(int bottom);

  // Layout the Extension Shelf, returns the coordinate of the top of the
  // control, for laying out the previous control.
  int LayoutExtensionShelf(int bottom);

  // Child views that the layout manager manages.
  TabStrip* tabstrip_;
  ToolbarView* toolbar_;
  views::View* contents_split_;
  views::View* contents_container_;
  views::View* infobar_container_;
  DownloadShelfView* download_shelf_;
  ExtensionShelf* extension_shelf_;
  BookmarkBarView* active_bookmark_bar_;

  BrowserView* browser_view_;

  // The distance the FindBar is from the top of the window, in pixels.
  int find_bar_y_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserViewLayoutManager);
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_CHROME_BROWSER_VIEW_LAYOUT_MANAGER_H_
