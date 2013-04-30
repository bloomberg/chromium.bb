// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_APP_LIST_CONTROLLER_OBSERVER_H_
#define ASH_WM_APP_LIST_CONTROLLER_OBSERVER_H_

namespace aura {
class RootWindow;
}

namespace ash {
namespace internal {

class AppListControllerObserver {
 public:
  // Invoked when app launcher bubble is toggled on the given root window.
  virtual void OnAppLauncherVisibilityChanged(
      bool visible,
      const aura::RootWindow* root_window) = 0;

 protected:
  virtual ~AppListControllerObserver() {}
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_APP_LIST_CONTROLLER_OBSERVER_H_
