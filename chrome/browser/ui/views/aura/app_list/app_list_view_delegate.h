// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura_shell/app_list/app_list_view_delegate.h"

class AppListViewDelegate : public aura_shell::AppListViewDelegate {
 public:
  AppListViewDelegate();
  virtual ~AppListViewDelegate();

 private:
  // Overridden from aura_shell::AppListViewDelegate:
  virtual void OnAppListItemActivated(aura_shell::AppListItemModel* item,
                                      int event_flags) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
