// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_TESTING_VIEWS_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_TESTING_VIEWS_H_

class BookmarkBarView;
class LocationBarView;
class ToolbarView;

namespace views {
class View;
}

// A BrowserWindow utility interface used for accessing elements of the browser
// UI used only by UI test automation.
class BrowserWindowTesting {
 public:
  // Returns the BookmarkBarView.
  virtual BookmarkBarView* GetBookmarkBarView() const = 0;

  // Returns the LocationBarView.
  virtual LocationBarView* GetLocationBarView() const = 0;

  // Returns the TabContentsContainer.
  virtual views::View* GetTabContentsContainerView() const = 0;

  // Returns the ToolbarView.
  virtual ToolbarView* GetToolbarView() const = 0;

 protected:
  virtual ~BrowserWindowTesting() {}
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_TESTING_VIEWS_H_
