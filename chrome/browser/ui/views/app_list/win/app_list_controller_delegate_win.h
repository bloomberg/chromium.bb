// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_views.h"

// Windows specific configuration and behaviour for the AppList.
class AppListControllerDelegateWin : public AppListControllerDelegateViews {
 public:
  explicit AppListControllerDelegateWin(AppListServiceViews* service);
  ~AppListControllerDelegateWin() override;

  // AppListControllerDelegate overrides:
  bool ForceNativeDesktop() const override;
  gfx::ImageSkia GetWindowIcon() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListControllerDelegateWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_CONTROLLER_DELEGATE_WIN_H_
