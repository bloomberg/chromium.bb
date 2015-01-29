// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_

#include <string>

#include "ui/gfx/native_widget_types.h"

class Browser;
class ToolbarActionsBar;
class ToolbarActionsBarDelegate;

namespace gfx {
class Image;
class Rect;
class Size;
}  // namespace gfx

class BrowserActionTestUtil {
 public:
  // Constructs a BrowserActionTestUtil that uses the |browser|'s default
  // browser action container.
  explicit BrowserActionTestUtil(Browser* browser)
      : browser_(browser), bar_delegate_(nullptr) {}

  // Constructs a BrowserActionTestUtil that will use the |bar_delegate| as the
  // browser action container to test.
  BrowserActionTestUtil(Browser* browser,
                        ToolbarActionsBarDelegate* bar_delegate)
      : browser_(browser), bar_delegate_(bar_delegate) {}

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

  // Tests that the overflow button is displaying an overflowed action wants
  // to run.
  bool OverflowedActionButtonWantsToRun();

  // Returns the ToolbarActionsBar.
  ToolbarActionsBar* GetToolbarActionsBar();

  // Returns the minimum allowed size of an extension popup.
  static gfx::Size GetMinPopupSize();

  // Returns the maximum allowed size of an extension popup.
  static gfx::Size GetMaxPopupSize();

 private:
  Browser* browser_;  // weak

  // If non-null, this is a set view to test with, rather than using the
  // |browser|'s default container.
  ToolbarActionsBarDelegate* bar_delegate_;  // weak
};

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
