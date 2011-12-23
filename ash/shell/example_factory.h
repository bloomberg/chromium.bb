// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_EXAMPLE_FACTORY_H_
#define ASH_SHELL_EXAMPLE_FACTORY_H_
#pragma once

#include "ui/aura_shell/shell_delegate.h"

namespace aura_shell {
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

void BuildAppListModel(aura_shell::AppListModel* model);

aura_shell::AppListViewDelegate* CreateAppListViewDelegate();

void CreateAppList(
    const gfx::Rect& bounds,
    const aura_shell::ShellDelegate::SetWidgetCallback& callback);

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_EXAMPLE_FACTORY_H_
