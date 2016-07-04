// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_helper.h"

#include "ash/mus/root_window_controller.h"
#include "ash/mus/test/wm_test_screen.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/tests/window_tree_client_private.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/display/display.h"

namespace ash {
namespace mus {

WmTestHelper::WmTestHelper() {}

WmTestHelper::~WmTestHelper() {}

void WmTestHelper::Init() {
  message_loop_.reset(new base::MessageLoopForUI());
  window_manager_app_.window_manager_.reset(new WindowManager(nullptr));
  screen_ = new WmTestScreen;
  window_manager_app_.window_manager_->screen_.reset(screen_);

  // Need an id other than kInvalidDisplayID so the Display is considered valid.
  display::Display display(1);
  const gfx::Rect display_bounds(0, 0, 800, 600);
  display.set_bounds(display_bounds);
  // Offset the height slightly to give a different work area. -20 is arbitrary,
  // it could be anything.
  const gfx::Rect work_area(0, 0, display_bounds.width(),
                            display_bounds.height() - 20);
  display.set_work_area(work_area);
  window_tree_client_setup_.InitForWindowManager(
      window_manager_app_.window_manager_.get(),
      window_manager_app_.window_manager_.get(), display);

  window_manager_app_.InitWindowManager(
      window_tree_client_setup_.window_tree_client());

  screen_->display_list()->AddDisplay(display,
                                      views::DisplayList::Type::PRIMARY);

  ::ui::WindowTreeClientPrivate(window_tree_client_setup_.window_tree_client())
      .CallWmNewDisplayAdded(display);
}

}  // namespace mus
}  // namespace ash
