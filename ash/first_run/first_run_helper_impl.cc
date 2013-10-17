// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper_impl.h"

#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

namespace ash {

FirstRunHelperImpl::FirstRunHelperImpl() {}

void FirstRunHelperImpl::OpenAppList() {
  if (Shell::GetInstance()->GetAppListTargetVisibility())
    return;
  Shell::GetInstance()->ToggleAppList(NULL);
}

void FirstRunHelperImpl::CloseAppList() {
  if (!Shell::GetInstance()->GetAppListTargetVisibility())
    return;
  Shell::GetInstance()->ToggleAppList(NULL);
}

gfx::Rect FirstRunHelperImpl::GetLauncherBounds() {
  ash::Launcher* launcher = ash::Launcher::ForPrimaryDisplay();
  return launcher->GetVisibleItemsBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetAppListButtonBounds() {
  ash::Launcher* launcher = ash::Launcher::ForPrimaryDisplay();
  views::View* app_button = launcher->GetAppListButtonView();
  return app_button->GetBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::GetAppListBounds() {
  app_list::AppListView* view = Shell::GetInstance()->GetAppListView();
  CHECK(view);
  return view->GetBoundsInScreen();
}

}  // namespace ash

