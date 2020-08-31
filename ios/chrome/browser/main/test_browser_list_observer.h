// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_LIST_OBSERVER_H_

#include <set>

#include "base/macros.h"
#import "ios/chrome/browser/main/browser_list_observer.h"

class Browser;
class BrowserList;

class TestBrowserListObserver : public BrowserListObserver {
 public:
  TestBrowserListObserver();
  ~TestBrowserListObserver() override;

  // A weak pointer to the last Browser that was observed being added to the
  // BrowserList's regular browsers.
  Browser* GetLastAddedBrowser() { return last_added_browser_; }
  // A weak pointer to the last Browser that was observed being added to the
  // BrowserList's incognito browsers.
  Browser* GetLastRemovedBrowser() { return last_removed_browser_; }
  // A weak pointer to the last Browser that was observed being removed from the
  // BrowserList's regular browsers.
  Browser* GetLastAddedIncognitoBrowser() {
    return last_added_incognito_browser_;
  }
  // A weak pointer to the last Browser that was observed being removed from the
  // BrowserList's incognito browsers.
  Browser* GetLastRemovedIncognitoBrowser() {
    return last_removed_incognito_browser_;
  }
  // The set of regular browsers that were in the browser list when the last
  // observed event occurred.
  std::set<Browser*> GetLastBrowsers() { return last_browsers_; }
  // The set of incognito browsers that were in the browser list when the last
  // observed event occurred.
  std::set<Browser*> GetLastIncognitoBrowsers() {
    return last_incognito_browsers_;
  }

  // BrowserListObserver
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnIncognitoBrowserAdded(const BrowserList* browser_list,
                               Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnIncognitoBrowserRemoved(const BrowserList* browser_list,
                                 Browser* browser) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserListObserver);

  // Backing vars for the corresponding getter methods.
  Browser* last_added_browser_;
  Browser* last_removed_browser_;
  Browser* last_added_incognito_browser_;
  Browser* last_removed_incognito_browser_;
  std::set<Browser*> last_browsers_;
  std::set<Browser*> last_incognito_browsers_;
};

#endif  // IOS_CHROME_BROWSER_MAIN_TEST_BROWSER_LIST_OBSERVER_H_
