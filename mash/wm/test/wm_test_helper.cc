// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/test/wm_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/root_window_controller.h"
#include "mash/wm/test/wm_test_screen.h"
#include "mash/wm/window_manager.h"
#include "mash/wm/window_manager.h"
#include "mash/wm/window_manager_application.h"
#include "ui/display/display.h"

namespace mash {
namespace wm {

WmTestHelper::WmTestHelper() {}

WmTestHelper::~WmTestHelper() {}

void WmTestHelper::Init() {
  message_loop_.reset(new base::MessageLoopForUI());
  const uint32_t app_id = 1;
  window_manager_app_.Initialize(nullptr, shell::Identity(), app_id);
  screen_ = new WmTestScreen;
  window_manager_app_.screen_.reset(screen_);

  // RootWindowController controls its own lifetime.
  RootWindowController* root_window_controller =
      new RootWindowController(&window_manager_app_);
  // Need an id other than kInvalidDisplayID so the Display is considered valid.
  root_window_controller->display_.set_id(1);
  const gfx::Rect display_bounds(0, 0, 800, 600);
  root_window_controller->display_.set_bounds(display_bounds);
  // Offset the height slightly to give a different work area. -20 is arbitrary,
  // it could be anything.
  const gfx::Rect work_area(0, 0, display_bounds.width(),
                            display_bounds.height() - 20);
  root_window_controller->display_.set_work_area(work_area);

  screen_->display_list()->AddDisplay(root_window_controller->display(),
                                      views::DisplayList::Type::PRIMARY);

  window_tree_connection_setup_.Init(
      root_window_controller, root_window_controller->window_manager_.get());
  root_window_controller->root()->SetBounds(display_bounds);
  window_manager_app_.AddRootWindowController(root_window_controller);
}

}  // namespace wm
}  // namespace mash
