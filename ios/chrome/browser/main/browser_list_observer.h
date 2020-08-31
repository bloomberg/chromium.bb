// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_OBSERVER_H_

#include "base/macros.h"

class Browser;
class BrowserList;

// Observer interface for BrowserList.
class BrowserListObserver {
 public:
  BrowserListObserver() = default;
  virtual ~BrowserListObserver() = default;

  // Called after |browser| is added to |browser_list|.
  virtual void OnBrowserAdded(const BrowserList* browser_list,
                              Browser* browser) {}
  virtual void OnIncognitoBrowserAdded(const BrowserList* browser_list,
                                       Browser* browser) {}

  // Called *after* |browser| is removed from |browser_list|. This method will
  // execute before the object that owns |browser| destroys it, so the pointer
  // passed here will be valid for these method calls, but it can't be used for
  // any processing outside of the synchronous scope of these methods.
  virtual void OnBrowserRemoved(const BrowserList* browser_list,
                                Browser* browser) {}
  virtual void OnIncognitoBrowserRemoved(const BrowserList* browser_list,
                                         Browser* browser) {}

  // Called before the browserlist is destroyed, in case the observer needs to
  // do any cleanup. After this method is called, all observers will be removed
  // from |browser_list|, and no firther obeserver methods will be called.
  virtual void OnBrowserListShutdown(BrowserList* browser_list) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserListObserver);
};

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_LIST_OBSERVER_H_
