// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_LIST_H_
#define IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_LIST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "ios/shared/chrome/browser/ui/browser_list/browser.h"

namespace ios {
class ChromeBrowserState;
}

// BrowserList attaches Browsers instance to a ChromeBrowserState.
class BrowserList : public base::SupportsUserData::Data {
 public:
  explicit BrowserList(ios::ChromeBrowserState* browser_state);
  ~BrowserList() override;

  static BrowserList* FromBrowserState(ios::ChromeBrowserState* browser_state);

  // Returns the number of Browsers in the BrowserList.
  int count() const { return static_cast<int>(browsers_.size()); }

  // Returns whether the specified index is valid.
  int ContainsIndex(int index) const;

  // Returns the Browser at the specified index.
  Browser* GetBrowserAtIndex(int index) const;

  // Returns the index of the specified Browser, or kInvalidIndex if not found.
  int GetIndexOfBrowser(const Browser* browser) const;

  // Creates and returns a new Browser instance.
  Browser* CreateNewBrowser();

  // Closes the Browser at the specified index.
  void CloseBrowserAtIndex(int index);

  // Invalid index.
  static const int kInvalidIndex = -1;

 private:
  ios::ChromeBrowserState* browser_state_;
  std::vector<std::unique_ptr<Browser>> browsers_;

  DISALLOW_COPY_AND_ASSIGN(BrowserList);
};

#endif  // IOS_SHARED_CHROME_BROWSER_UI_BROWSER_LIST_BROWSER_LIST_H_
