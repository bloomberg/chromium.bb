// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_DELEGATE_H_
#define ASH_SHELF_HOME_BUTTON_DELEGATE_H_

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"

namespace ash {

// ShelfItemDelegate for the Home button, aka the app list button.
// TODO(michaelpg): Rename references to app list button to home button.
class ASH_EXPORT HomeButtonDelegate : public ShelfItemDelegate {
 public:
  HomeButtonDelegate();
  ~HomeButtonDelegate() override;

  // Responds to the home button being activated or the equivalent accelerator
  // being pressed.
  // |display_id| is the display of the button, indicating where the Home screen
  // should be shown.
  // |show_source| is the type of user action that triggered this.
  // |event_time_stamp| is timestamp of the triggering event.
  static ShelfAction PerformHomeButtonAction(
      int64_t display_id,
      app_list::AppListShowSource show_source,
      base::TimeTicks event_time_stamp);

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override;
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeButtonDelegate);
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_DELEGATE_H_
