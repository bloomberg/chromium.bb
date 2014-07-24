// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_OBSERVER_H_

class BrowserActionsContainerObserver {
 public:
  virtual void OnBrowserActionsContainerAnimationEnded() {}
  virtual void OnBrowserActionsContainerDestroyed() {}

 protected:
  virtual ~BrowserActionsContainerObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_BROWSER_ACTIONS_CONTAINER_OBSERVER_H_
