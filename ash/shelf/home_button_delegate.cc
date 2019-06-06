// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button_delegate.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/kiosk_next/kiosk_next_shell_controller.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shell.h"
#include "base/logging.h"

namespace ash {

HomeButtonDelegate::HomeButtonDelegate()
    : ShelfItemDelegate(ShelfID(kAppListId)) {}

HomeButtonDelegate::~HomeButtonDelegate() = default;

// static
ShelfAction HomeButtonDelegate::PerformHomeButtonAction(
    int64_t display_id,
    app_list::AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  ShelfAction shelf_action = ash::SHELF_ACTION_NONE;
  if (Shell::Get()->kiosk_next_shell_controller()->IsEnabled()) {
    Shell::Get()->home_screen_controller()->GoHome(display_id);
    shelf_action = ash::SHELF_ACTION_APP_LIST_SHOWN;
  } else {
    shelf_action = Shell::Get()->app_list_controller()->OnAppListButtonPressed(
        display_id, show_source, event_time_stamp);
  }
  return shelf_action;
}

void HomeButtonDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                      int64_t display_id,
                                      ShelfLaunchSource source,
                                      ItemSelectedCallback callback) {
  ShelfAction shelf_action = PerformHomeButtonAction(
      display_id,
      event->IsShiftDown() ? app_list::kShelfButtonFullscreen
                           : app_list::kShelfButton,
      event->time_stamp());
  std::move(callback).Run(shelf_action, {});
}

void HomeButtonDelegate::ExecuteCommand(bool from_context_menu,
                                        int64_t command_id,
                                        int32_t event_flags,
                                        int64_t display_id) {
  // This delegate does not show custom context or application menu items.
  NOTIMPLEMENTED();
}

void HomeButtonDelegate::Close() {}

}  // namespace ash
