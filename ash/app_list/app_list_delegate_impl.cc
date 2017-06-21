// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_delegate_impl.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/presenter/app_list.h"

namespace ash {
// TODO(newcomer): Remove this class as a part of crbug.com/726838
AppListDelegateImpl::AppListDelegateImpl() {
  Shell::Get()->app_list()->set_delegate(this);
}

AppListDelegateImpl::~AppListDelegateImpl() {
  Shell::Get()->app_list()->set_delegate(nullptr);
}

void AppListDelegateImpl::OnAppListVisibilityChanged(bool visible,
                                                     int64_t display_id) {
  if (app_list::features::IsFullscreenAppListEnabled()) {
    aura::Window* root_window =
        Shell::Get()->GetRootWindowForDisplayId(display_id);
    Shell::Get()->OnAppListVisibilityChanged(visible, root_window);
  }
}

}  // namespace ash
