// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_

#include <string>

#include "build/build_config.h"

class Browser;
class ExtensionAction;

namespace gfx {
class Image;
class Rect;
class Size;
}  // namespace gfx

class BrowserActionTestUtil {
 public:
  explicit BrowserActionTestUtil(Browser* browser) : browser_(browser) {}

  // Returns the number of browser action buttons in the window toolbar.
  int NumberOfBrowserActions();

  // Returns the number of browser action currently visible.
  int VisibleBrowserActions();

#if defined(TOOLKIT_VIEWS)
  // Returns the ExtensionAction for the given index.
  ExtensionAction* GetExtensionAction(int index);
#endif

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

  // Returns whether a browser action popup is being shown currently.
  bool HasPopup();

  // Returns the bounds of the current browser action popup.
  gfx::Rect GetPopupBounds();

  // Hides the given popup and returns whether the hide was successful.
  bool HidePopup();

  // Set how many icons should be visible.
  void SetIconVisibilityCount(size_t icons);

  // Returns the minimum allowed size of an extension popup.
  static gfx::Size GetMinPopupSize();

  // Returns the maximum allowed size of an extension popup.
  static gfx::Size GetMaxPopupSize();

 private:
  Browser* browser_;  // weak
};

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
