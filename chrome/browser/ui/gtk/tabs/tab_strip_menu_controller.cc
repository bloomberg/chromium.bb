// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tabs/tab_strip_menu_controller.h"

#include "chrome/browser/ui/gtk/accelerators_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/tabs/tab_gtk.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

TabStripMenuController::TabStripMenuController(TabGtk* tab,
                                               TabStripModel* model,
                                               int index)
    : tab_(tab),
      model_(this, model, index) {
  menu_.reset(new MenuGtk(this, &model_));
}

TabStripMenuController::~TabStripMenuController() {}

void TabStripMenuController::RunMenu(const gfx::Point& point,
                                     guint32 event_time) {
  menu_->PopupAsContext(point, event_time);
}

void TabStripMenuController::Cancel() {
  tab_ = NULL;
  menu_->Cancel();
}

bool TabStripMenuController::IsCommandIdChecked(int command_id) const {
  return false;
}

bool TabStripMenuController::IsCommandIdEnabled(int command_id) const {
  return tab_ && tab_->delegate()->IsCommandEnabledForTab(
      static_cast<TabStripModel::ContextMenuCommand>(command_id),
      tab_);
}

bool TabStripMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  int browser_command;
  if (!TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                         &browser_command))
    return false;
  const ui::AcceleratorGtk* accelerator_gtk =
      AcceleratorsGtk::GetInstance()->GetPrimaryAcceleratorForCommand(
          browser_command);
  if (accelerator_gtk)
    *accelerator = *accelerator_gtk;
  return !!accelerator_gtk;
}

void TabStripMenuController::ExecuteCommand(int command_id) {
  // Checking if the tab still exists since it is possible that the tab
  // corresponding to this context menu has been closed.
  if (!tab_)
    return;
  tab_->delegate()->ExecuteCommandForTab(
      static_cast<TabStripModel::ContextMenuCommand>(command_id), tab_);
}

GtkWidget* TabStripMenuController::GetImageForCommandId(int command_id) const {
  int browser_cmd_id;
  if (!TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                         &browser_cmd_id))
    return NULL;
  return MenuGtk::Delegate::GetDefaultImageForCommandId(browser_cmd_id);
}
