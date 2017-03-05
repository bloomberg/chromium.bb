// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_helper.h"

#include "ash/common/test/wm_shell_test_api.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "ash/test/test_shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/window.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace mus {
namespace {

bool CompareByDisplayId(RootWindowController* root1,
                        RootWindowController* root2) {
  return root1->GetWindow()->GetDisplayNearestWindow().id() <
         root2->GetWindow()->GetDisplayNearestWindow().id();
}

// TODO(sky): at some point this needs to support everything in DisplayInfo,
// for now just the bare minimum, which is [x+y-]wxh.
gfx::Rect ParseDisplayBounds(const std::string& spec) {
  gfx::Rect bounds;
  const std::vector<std::string> parts =
      base::SplitString(spec, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string size_spec;
  if (parts.size() == 2u) {
    size_spec = parts[1];
    const std::vector<std::string> origin_parts = base::SplitString(
        parts[0], "+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK_EQ(2u, origin_parts.size());
    int x, y;
    CHECK(base::StringToInt(origin_parts[0], &x));
    CHECK(base::StringToInt(origin_parts[1], &y));
    bounds.set_origin(gfx::Point(x, y));
  } else {
    CHECK_EQ(1u, parts.size());
    size_spec = spec;
  }
  const std::vector<std::string> size_parts = base::SplitString(
      size_spec, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  CHECK_EQ(2u, size_parts.size());
  int w = 0, h = 0;
  CHECK(base::StringToInt(size_parts[0], &w));
  CHECK(base::StringToInt(size_parts[1], &h));
  bounds.set_size(gfx::Size(w, h));
  return bounds;
}

}  // namespace

WmTestHelper::WmTestHelper() {}

WmTestHelper::~WmTestHelper() {
  // Needs to be destroyed before material design.
  window_manager_app_.reset();

  base::RunLoop().RunUntilIdle();
  blocking_pool_owner_.reset();
  base::RunLoop().RunUntilIdle();

  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
}

void WmTestHelper::Init() {
  // MaterialDesignController may have already been initialized. To cover that
  // case explicitly uninitialize before initializing.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
  ui::MaterialDesignController::Initialize();

  views_delegate_ = base::MakeUnique<views::TestViewsDelegate>();

  window_manager_app_ = base::MakeUnique<WindowManagerApplication>();

  message_loop_.reset(new base::MessageLoopForUI());

  const size_t kMaxNumberThreads = 3u;  // Matches that of content.
  const char kThreadNamePrefix[] = "MashBlockingForTesting";
  blocking_pool_owner_ = base::MakeUnique<base::SequencedWorkerPoolOwner>(
      kMaxNumberThreads, kThreadNamePrefix);

  window_manager_app_->window_manager_.reset(new WindowManager(nullptr));
  window_manager_app_->window_manager()->shell_delegate_for_test_ =
      base::MakeUnique<test::TestShellDelegate>();

  window_tree_client_setup_.InitForWindowManager(
      window_manager_app_->window_manager_.get(),
      window_manager_app_->window_manager_.get());
  aura::test::EnvTestHelper().SetWindowTreeClient(
      window_tree_client_setup_.window_tree_client());
  window_manager_app_->InitWindowManager(
      window_tree_client_setup_.OwnWindowTreeClient(),
      blocking_pool_owner_->pool());

  aura::WindowTreeClient* window_tree_client =
      window_manager_app_->window_manager()->window_tree_client();
  window_tree_client_private_ =
      base::MakeUnique<aura::WindowTreeClientPrivate>(window_tree_client);
  int next_x = 0;
  CreateRootWindowController("800x600", &next_x);
}

std::vector<RootWindowController*> WmTestHelper::GetRootsOrderedByDisplayId() {
  std::set<RootWindowController*> roots =
      window_manager_app_->window_manager()->GetRootWindowControllers();
  std::vector<RootWindowController*> ordered_roots;
  ordered_roots.insert(ordered_roots.begin(), roots.begin(), roots.end());
  std::sort(ordered_roots.begin(), ordered_roots.end(), &CompareByDisplayId);
  return ordered_roots;
}

void WmTestHelper::UpdateDisplay(const std::string& display_spec) {
  const std::vector<std::string> parts = base::SplitString(
      display_spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<RootWindowController*> root_window_controllers =
      GetRootsOrderedByDisplayId();
  int next_x = 0;
  for (size_t i = 0,
              end = std::min(parts.size(), root_window_controllers.size());
       i < end; ++i) {
    UpdateDisplay(root_window_controllers[i], parts[i], &next_x);
  }
  for (size_t i = root_window_controllers.size(); i < parts.size(); ++i) {
    root_window_controllers.push_back(
        CreateRootWindowController(parts[i], &next_x));
  }
  const bool in_shutdown = false;
  while (root_window_controllers.size() > parts.size()) {
    window_manager_app_->window_manager()->DestroyRootWindowController(
        root_window_controllers.back(), in_shutdown);
    root_window_controllers.pop_back();
  }
}

RootWindowController* WmTestHelper::CreateRootWindowController(
    const std::string& display_spec,
    int* next_x) {
  gfx::Rect bounds = ParseDisplayBounds(display_spec);
  bounds.set_x(*next_x);
  *next_x += bounds.size().width();
  display::Display display(next_display_id_++, bounds);
  gfx::Rect work_area(display.bounds());
  // Offset the height slightly to give a different work area. -20 is arbitrary,
  // it could be anything.
  work_area.set_height(std::max(0, work_area.height() - 20));
  display.set_work_area(work_area);
  window_tree_client_private_->CallWmNewDisplayAdded(display);
  return GetRootsOrderedByDisplayId().back();
}

void WmTestHelper::UpdateDisplay(RootWindowController* root_window_controller,
                                 const std::string& display_spec,
                                 int* next_x) {
  gfx::Rect bounds = ParseDisplayBounds(display_spec);
  bounds.set_x(*next_x);
  *next_x += bounds.size().width();
  display::Display updated_display =
      root_window_controller->GetWindow()->GetDisplayNearestWindow();
  gfx::Insets work_area_insets = updated_display.GetWorkAreaInsets();
  updated_display.set_bounds(bounds);
  updated_display.UpdateWorkAreaFromInsets(work_area_insets);
  root_window_controller->GetWindow()->SetBounds(gfx::Rect(bounds.size()));
  ScreenMus* screen = window_manager_app_->window_manager()->screen_.get();
  const bool is_primary =
      screen->display_list().FindDisplayById(updated_display.id()) ==
      screen->display_list().GetPrimaryDisplayIterator();
  screen->display_list().UpdateDisplay(
      updated_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

}  // namespace mus
}  // namespace ash
