// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_

namespace app_list {
class AppListView;
}

class AppListViewDelegate;

// Allows platform-specific hooks for the AppListShower.
class AppListShowerDelegate {
 public:
  virtual AppListViewDelegate* GetViewDelegateForCreate() = 0;
  virtual void OnViewCreated() = 0;
  virtual void OnViewDismissed() = 0;
  virtual void MoveNearCursor(app_list::AppListView* view) = 0;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SHOWER_DELEGATE_H_
