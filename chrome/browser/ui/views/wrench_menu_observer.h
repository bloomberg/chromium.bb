// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_OBSERVER_H_

class WrenchMenuObserver {
 public:
  // Invoked when the WrenchMenu is about to be destroyed (from its destructor).
  virtual void WrenchMenuDestroyed() = 0;

 protected:
  virtual ~WrenchMenuObserver() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_WRENCH_MENU_OBSERVER_H_
