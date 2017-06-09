// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_delegate_impl.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {

AppListDelegateImpl::AppListDelegateImpl() {
  Shell::Get()->app_list()->set_delegate(this);
}

AppListDelegateImpl::~AppListDelegateImpl() {
  Shell::Get()->app_list()->set_delegate(nullptr);
}

void AppListDelegateImpl::OnAppListVisibilityChanged(bool visible,
                                                     int64_t display_id) {
  aura::Window* root_window =
      ShellPort::Get()->GetRootWindowForDisplayId(display_id);
  AppListButton* app_list_button =
      Shelf::ForWindow(root_window)->shelf_widget()->GetAppListButton();
  if (!app_list_button)
    return;

  if (visible)
    app_list_button->OnAppListShown();
  else
    app_list_button->OnAppListDismissed();
}

}  // namespace ash
