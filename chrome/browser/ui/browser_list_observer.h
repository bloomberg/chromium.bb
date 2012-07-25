// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_
#define CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_

class Browser;

namespace chrome {

class BrowserListObserver {
  public:
  // Called immediately after a browser is added to the list
  virtual void OnBrowserAdded(Browser* browser) {}

  // Called immediately after a browser is removed from the list
  virtual void OnBrowserRemoved(Browser* browser) {}

  // Called immediately after a browser is set active (SetLastActive)
  virtual void OnBrowserSetLastActive(Browser* browser) {}

 protected:
  virtual ~BrowserListObserver() {}
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_LIST_OBSERVER_H_
