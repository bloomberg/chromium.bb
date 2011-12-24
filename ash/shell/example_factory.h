// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_EXAMPLE_FACTORY_H_
#define ASH_SHELL_EXAMPLE_FACTORY_H_
#pragma once

#include "ash/shell_delegate.h"

namespace ash {
class AppListModel;
class AppListViewDelegate;
}

namespace views {
class View;
}

namespace ash {
namespace shell {

void CreatePointyBubble(views::View* anchor_view);

void CreateLockScreen();

// Creates a window showing samples of commonly used widgets.
void CreateWidgetsWindow();

void BuildAppListModel(ash::AppListModel* model);

ash::AppListViewDelegate* CreateAppListViewDelegate();

void CreateAppList(
    const gfx::Rect& bounds,
    const ash::ShellDelegate::SetWidgetCallback& callback);

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_EXAMPLE_FACTORY_H_
