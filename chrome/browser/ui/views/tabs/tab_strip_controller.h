// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
#pragma once

class BaseTab;
class BaseTabStrip;
class GURL;

namespace gfx {
class Point;
}

// Model/Controller for the TabStrip.
// NOTE: All indices used by this class are in model coordinates.
class TabStripController {
 public:
  virtual ~TabStripController() {}

  // Returns the number of tabs in the model.
  virtual int GetCount() const = 0;

  // Returns true if |index| is a valid model index.
  virtual bool IsValidIndex(int index) const = 0;

  // Returns true if the selected index is selected.
  virtual bool IsTabSelected(int index) const = 0;

  // Returns true if the selected index is pinned.
  virtual bool IsTabPinned(int index) const = 0;

  // Returns true if the selected index is closeable.
  virtual bool IsTabCloseable(int index) const = 0;

  // Returns true if the selected index is the new tab page.
  virtual bool IsNewTabPage(int index) const = 0;

  // Select the tab at the specified index in the model.
  virtual void SelectTab(int index) = 0;

  // Closes the tab at the specified index in the model.
  virtual void CloseTab(int index) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenu(BaseTab* tab, const gfx::Point& p) = 0;

  // Updates the loading animations of all the tabs.
  virtual void UpdateLoadingAnimations() = 0;

  // Returns true if the associated TabStrip's delegate supports tab moving or
  // detaching. Used by the Frame to determine if dragging on the Tab
  // itself should move the window in cases where there's only one
  // non drag-able Tab.
  virtual int HasAvailableDragActions() const = 0;

  // Performans a drop at the specified location.
  virtual void PerformDrop(bool drop_before, int index, const GURL& url) = 0;

  // Return true if this tab strip is compatible with the provided tab strip.
  // Compatible tab strips can transfer tabs during drag and drop.
  virtual bool IsCompatibleWith(BaseTabStrip* other) const = 0;

  // Creates the new tab.
  virtual void CreateNewTab() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
