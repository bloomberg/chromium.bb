// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_delegate_impl.h"

#include "ash/common/shelf/app_list_button.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/root_window_controller.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {

AppListDelegateImpl::AppListDelegateImpl() {
  WmShell::Get()->app_list()->set_delegate(this);
}

AppListDelegateImpl::~AppListDelegateImpl() {
  WmShell::Get()->app_list()->set_delegate(nullptr);
}

void AppListDelegateImpl::OnAppListVisibilityChanged(bool visible,
                                                     int64_t display_id) {
  WmWindow* root_window = WmShell::Get()->GetRootWindowForDisplayId(display_id);
  AppListButton* app_list_button =
      WmShelf::ForWindow(root_window)->shelf_widget()->GetAppListButton();
  if (!app_list_button)
    return;

  if (visible)
    app_list_button->OnAppListShown();
  else
    app_list_button->OnAppListDismissed();
}

}  // namespace ash
