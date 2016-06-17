// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/test/wm_test_base.h"

#include <algorithm>
#include <vector>

#include "ash/mus/root_window_controller.h"
#include "ash/mus/test/wm_test_helper.h"
#include "ash/mus/test/wm_test_screen.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "ui/display/display.h"

namespace ash {
namespace mus {
namespace {

::mus::mojom::WindowType MusWindowTypeFromWmWindowType(
    ui::wm::WindowType wm_window_type) {
  switch (wm_window_type) {
    case ui::wm::WINDOW_TYPE_UNKNOWN:
      break;

    case ui::wm::WINDOW_TYPE_NORMAL:
      return ::mus::mojom::WindowType::WINDOW;

    case ui::wm::WINDOW_TYPE_POPUP:
      return ::mus::mojom::WindowType::POPUP;

    case ui::wm::WINDOW_TYPE_CONTROL:
      return ::mus::mojom::WindowType::CONTROL;

    case ui::wm::WINDOW_TYPE_PANEL:
      return ::mus::mojom::WindowType::PANEL;

    case ui::wm::WINDOW_TYPE_MENU:
      return ::mus::mojom::WindowType::MENU;

    case ui::wm::WINDOW_TYPE_TOOLTIP:
      return ::mus::mojom::WindowType::TOOLTIP;
  }

  NOTREACHED();
  return ::mus::mojom::WindowType::CONTROL;
}

bool CompareByDisplayId(const RootWindowController* root1,
                        const RootWindowController* root2) {
  return root1->display().id() < root2->display().id();
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
  int w, h;
  CHECK(base::StringToInt(size_parts[0], &w));
  CHECK(base::StringToInt(size_parts[1], &h));
  bounds.set_size(gfx::Size(w, h));
  return bounds;
}

}  // namespace

WmTestBase::WmTestBase() {}

WmTestBase::~WmTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called WmTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called WmTestBase::TearDown";
}

bool WmTestBase::SupportsMultipleDisplays() const {
  return false;
}

void WmTestBase::UpdateDisplay(const std::string& display_spec) {
  const std::vector<std::string> parts = base::SplitString(
      display_spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // TODO(sky): allow > 1 once http://crbug.com/611563 is fixed.
  DCHECK_EQ(1u, parts.size());
  gfx::Rect bounds = ParseDisplayBounds(parts[0]);
  std::vector<RootWindowController*> roots =
      WmTestBase::GetRootsOrderedByDisplayId();
  roots[0]->display_.set_bounds(bounds);
  gfx::Rect work_area(bounds);
  // Offset the height slightly to give a different work area. -2 is arbitrary,
  // it could be anything.
  work_area.set_height(std::max(0, work_area.height() - 2));
  roots[0]->display_.set_work_area(work_area);
  roots[0]->root()->SetBounds(gfx::Rect(bounds.size()));
  test_helper_->screen()->display_list()->UpdateDisplay(
      roots[0]->display(), views::DisplayList::Type::PRIMARY);
}

::mus::Window* WmTestBase::GetPrimaryRootWindow() {
  std::vector<RootWindowController*> roots =
      WmTestBase::GetRootsOrderedByDisplayId();
  DCHECK(!roots.empty());
  return roots[0]->root();
}

::mus::Window* WmTestBase::GetSecondaryRootWindow() {
  std::vector<RootWindowController*> roots =
      WmTestBase::GetRootsOrderedByDisplayId();
  return roots.size() < 2 ? nullptr : roots[1]->root();
}

display::Display WmTestBase::GetPrimaryDisplay() {
  std::vector<RootWindowController*> roots =
      WmTestBase::GetRootsOrderedByDisplayId();
  DCHECK(!roots.empty());
  return roots[0]->display();
}

display::Display WmTestBase::GetSecondaryDisplay() {
  std::vector<RootWindowController*> roots =
      WmTestBase::GetRootsOrderedByDisplayId();
  return roots.size() < 2 ? display::Display() : roots[1]->display();
}

::mus::Window* WmTestBase::CreateTestWindow(const gfx::Rect& bounds) {
  return CreateTestWindow(bounds, ui::wm::WINDOW_TYPE_NORMAL);
}

::mus::Window* WmTestBase::CreateTestWindow(const gfx::Rect& bounds,
                                            ui::wm::WindowType window_type) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[::mus::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(MusWindowTypeFromWmWindowType(window_type)));
  if (!bounds.IsEmpty()) {
    properties[::mus::mojom::WindowManager::kInitialBounds_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(bounds);
  }
  properties[::mus::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          ::mus::mojom::kResizeBehaviorCanResize |
          ::mus::mojom::kResizeBehaviorCanMaximize |
          ::mus::mojom::kResizeBehaviorCanMinimize);

  ::mus::Window* window =
      GetRootsOrderedByDisplayId()[0]->window_manager()->NewTopLevelWindow(
          &properties);
  window->SetVisible(true);
  return window;
}

::mus::Window* WmTestBase::CreateChildTestWindow(::mus::Window* parent,
                                                 const gfx::Rect& bounds) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[::mus::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int32_t>(
          MusWindowTypeFromWmWindowType(ui::wm::WINDOW_TYPE_NORMAL)));
  ::mus::Window* window =
      GetRootsOrderedByDisplayId()[0]->root()->window_tree()->NewWindow(
          &properties);
  window->SetBounds(bounds);
  window->SetVisible(true);
  parent->AddChild(window);
  return window;
}

void WmTestBase::SetUp() {
  setup_called_ = true;
  test_helper_.reset(new WmTestHelper);
  test_helper_->Init();
}

void WmTestBase::TearDown() {
  teardown_called_ = true;
  test_helper_.reset();
}

std::vector<RootWindowController*> WmTestBase::GetRootsOrderedByDisplayId() {
  std::set<RootWindowController*> roots = test_helper_->window_manager_app()
                                              ->window_manager()
                                              ->GetRootWindowControllers();
  std::vector<RootWindowController*> ordered_roots;
  ordered_roots.insert(ordered_roots.begin(), roots.begin(), roots.end());
  std::sort(ordered_roots.begin(), ordered_roots.end(), &CompareByDisplayId);
  return ordered_roots;
}

}  // namespace mus
}  // namespace ash
