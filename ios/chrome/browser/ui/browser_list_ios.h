// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_LIST_IOS_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_LIST_IOS_H_

#include <vector>

#import "ios/chrome/browser/ui/browser_ios.h"

namespace ios {
class ChromeBrowserState;
}

// Modeled after chrome/browser/ui/browser_list.

class BrowserListIOS {
 public:
  typedef std::vector<id<BrowserIOS>> BrowserVector;
  typedef BrowserVector::iterator iterator;
  typedef BrowserVector::const_iterator const_iterator;

  // Note: browsers are not retained, just like desktop browser lists, their
  // management is outside the list.
  static void AddBrowser(id<BrowserIOS> browser);
  static void RemoveBrowser(id<BrowserIOS> browser);

  static id<BrowserIOS> GetLastActiveWithBrowserState(
      ios::ChromeBrowserState* browser_state);

  static const_iterator begin();
  static const_iterator end();

  static bool empty() { return !browsers_ || browsers_->empty(); }
  static size_t size() { return browsers_ ? browsers_->size() : 0; }

  // Returns true if at least one incognito session is active.
  static bool IsOffTheRecordSessionActive();

 private:
  static BrowserVector* browsers_;

  static void EnsureBrowsersIsValid();
};

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_LIST_IOS_H_
