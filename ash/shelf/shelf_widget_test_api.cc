// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shelf_widget_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ui/views/view.h"

namespace ash {

views::View* ShelfWidgetTestApi::GetHomeButton() {
  return Shell::GetRootWindowControllerWithDisplayId(
             display::Screen::GetScreen()->GetPrimaryDisplay().id())
      ->shelf()
      ->shelf_widget()
      ->navigation_widget()
      ->GetHomeButton();
}

}  // namespace ash
