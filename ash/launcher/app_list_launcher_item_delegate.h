// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_APP_LIST_LAUNCHER_ITEM_DELEGATE_H_
#define ASH_LAUNCHER_APP_LIST_LAUNCHER_ITEM_DELEGATE_H_

#include "ash/launcher/launcher_item_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

// LauncherItemDelegate for TYPE_APP_LIST.
class AppListLauncherItemDelegate : public LauncherItemDelegate {
 public:
  AppListLauncherItemDelegate();

  virtual ~AppListLauncherItemDelegate();

  // ash::LauncherItemDelegate overrides:
  virtual void ItemSelected(const LauncherItem& item,
                            const ui::Event& event) OVERRIDE;
  virtual base::string16 GetTitle(const LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const LauncherItem& item,
      aura::RootWindow* root_window) OVERRIDE;
  virtual LauncherMenuModel* CreateApplicationMenu(
      const LauncherItem& item,
      int event_flags) OVERRIDE;
  virtual bool IsDraggable(const LauncherItem& item) OVERRIDE;
  virtual bool ShouldShowTooltip(const LauncherItem& item) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListLauncherItemDelegate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_APP_LIST_LAUNCHER_ITEM_DELEGATE_H_
