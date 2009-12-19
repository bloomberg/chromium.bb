// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_

#include <string>

class Browser;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

class BrowserActionTestUtil {
 public:
  explicit BrowserActionTestUtil(Browser* browser) : browser_(browser) {}

  // Returns the number of browser action buttons in the window toolbar.
  int NumberOfBrowserActions();

  // Returns whether the browser action at |index| has a non-null icon.
  bool HasIcon(int index);

  // Simulates a user click on the browser action button at |index|.
  void Press(int index);

  // Returns the current tooltip for the browser action button.
  std::string GetTooltip(int index);

// TODO(estade): http://crbug.com/29710 port to Linux
#if !defined(TOOLKIT_GTK)
  // Returns whether a browser action popup is being shown currently.
  bool HasPopup();

  // Returns the bounds of the current browser action popup.
  gfx::Rect GetPopupBounds();

  // Hides the given popup and returns whether the hide was successful.
  bool HidePopup();

  // Returns the minimum allowed size of an extension popup.
  static gfx::Size GetMinPopupSize();

  // Returns the maximum allowed size of an extension popup.
  static gfx::Size GetMaxPopupSize();
#endif

 private:
  Browser* browser_;  // weak
};

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_ACTION_TEST_UTIL_H_
