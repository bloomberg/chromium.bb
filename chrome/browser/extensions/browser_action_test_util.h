// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class ToolbarActionsBar;
class ToolbarActionsBarDelegate;

namespace gfx {
class Image;
class Rect;
class Size;
}  // namespace gfx

// A class that creates and owns the platform-specific views for the browser
// actions container. Specific implementations are in the .cc/.mm files.
class TestToolbarActionsBarHelper {
 public:
  virtual ~TestToolbarActionsBarHelper() {}
};

class BrowserActionTestUtil {
 public:
  // Constructs a BrowserActionTestUtil that uses the |browser|'s default
  // browser action container.
  explicit BrowserActionTestUtil(Browser* browser);

  // Constructs a BrowserActionTestUtil which, if |is_real_window| is false,
  // will create its own browser actions container. This is useful in unit
  // tests, when the |browser|'s window doesn't create platform-specific views.
  BrowserActionTestUtil(Browser* browser, bool is_real_window);

  ~BrowserActionTestUtil();

  // Returns the number of browser action buttons in the window toolbar.
  int NumberOfBrowserActions();

  // Returns the number of browser action currently visible.
  int VisibleBrowserActions();

  // Returns true if the overflow chevron is completely shown in the browser
  // actions container (i.e., is visible and is within the bounds of the
  // container).
  bool IsChevronShowing();

  // Inspects the extension popup for the action at the given index.
  void InspectPopup(int index);

  // Returns whether the browser action at |index| has a non-null icon. Note
  // that the icon is loaded asynchronously, in which case you can wait for it
  // to load by calling WaitForBrowserActionUpdated.
  bool HasIcon(int index);

  // Returns icon for the browser action at |index|.
  gfx::Image GetIcon(int index);

  // Simulates a user click on the browser action button at |index|.
  void Press(int index);

  // Returns the extension id of the extension at |index|.
  std::string GetExtensionId(int index);

  // Returns the current tooltip for the browser action button.
  std::string GetTooltip(int index);

  gfx::NativeView GetPopupNativeView();

  // Returns whether a browser action popup is being shown currently.
  bool HasPopup();

  // Returns the size of the current browser action popup.
  gfx::Size GetPopupSize();

  // Hides the given popup and returns whether the hide was successful.
  bool HidePopup();

  // Tests that the button at the given |index| is displaying that it wants
  // to run.
  bool ActionButtonWantsToRun(size_t index);

  // Sets the current width of the browser actions container without resizing
  // the underlying controller. This is to simulate e.g. when the browser window
  // is too small for the preferred width.
  void SetWidth(int width);

  // Returns true if the container is currently highlighting in preparation for
  // showing the icon surfacing bubble.
  bool IsHighlightingForSurfacingBubble();

  // Returns the ToolbarActionsBar.
  ToolbarActionsBar* GetToolbarActionsBar();

  // Creates and returns a BrowserActionTestUtil with an "overflow" container,
  // with this object's container as the main bar.
  scoped_ptr<BrowserActionTestUtil> CreateOverflowBar();

  // Returns the minimum allowed size of an extension popup.
  static gfx::Size GetMinPopupSize();

  // Returns the maximum allowed size of an extension popup.
  static gfx::Size GetMaxPopupSize();

 private:
  // A private constructor to create an overflow version.
  BrowserActionTestUtil(Browser* browser, BrowserActionTestUtil* main_bar);

  Browser* browser_;  // weak

  // Our test helper, which constructs and owns the views if we don't have a
  // real browser window, or if this is an overflow version.
  scoped_ptr<TestToolbarActionsBarHelper> test_helper_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionTestUtil);
};

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
