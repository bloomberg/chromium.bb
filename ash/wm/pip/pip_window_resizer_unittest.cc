// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {
namespace wm {

namespace {

// WindowState based on a given initial state. Records the last resize bounds.
class FakeWindowState : public wm::WindowState::State {
 public:
  explicit FakeWindowState(mojom::WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  ~FakeWindowState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(wm::WindowState* window_state,
                 const wm::WMEvent* event) override {
    if (event->IsBoundsEvent()) {
      if (event->type() == wm::WM_EVENT_SET_BOUNDS) {
        const auto* set_bounds_event =
            static_cast<const wm::SetBoundsEvent*>(event);
        last_bounds_ = set_bounds_event->requested_bounds();
      }
    }
  }
  mojom::WindowStateType GetType() const override { return state_type_; }
  void AttachState(wm::WindowState* window_state,
                   wm::WindowState::State* previous_state) override {}
  void DetachState(wm::WindowState* window_state) override {}

  const gfx::Rect& last_bounds() { return last_bounds_; }

 private:
  mojom::WindowStateType state_type_;
  gfx::Rect last_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowState);
};

}  // namespace

class PipWindowResizerTest : public AshTestBase {
 public:
  PipWindowResizerTest() = default;
  ~PipWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    window_ = CreateTestWindowInShellWithBounds(gfx::Rect(200, 200, 100, 100));
    wm::WindowState* window_state = wm::GetWindowState(window_);
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    window_state->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
  }

  void TearDown() override { AshTestBase::TearDown(); }

 protected:
  aura::Window* window() { return window_; }
  FakeWindowState* test_state() { return test_state_; }

  PipWindowResizer* CreateResizerForTest(int window_component) {
    wm::WindowState* window_state = wm::GetWindowState(window());
    window_state->CreateDragDetails(gfx::Point(), window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE);
    return new PipWindowResizer(window_state);
  }

  gfx::Point CalculateDragPoint(const WindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 private:
  aura::Window* window_;
  FakeWindowState* test_state_;

  DISALLOW_COPY_AND_ASSIGN(PipWindowResizerTest);
};

TEST_F(PipWindowResizerTest, PipWindowCanDrag) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ("200,210 100x100", test_state()->last_bounds().ToString());
}

TEST_F(PipWindowResizerTest, PipWindowCanResize) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ("200,200 100x110", test_state()->last_bounds().ToString());
}

TEST_F(PipWindowResizerTest, PipWindowDragIsRestrictedToWorkArea) {
  UpdateWorkArea("400x400");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);
  EXPECT_EQ("300,200 100x100", test_state()->last_bounds().ToString());

  // Drag down.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 800), 0);
  EXPECT_EQ("200,300 100x100", test_state()->last_bounds().ToString());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -800, 0), 0);
  EXPECT_EQ("0,200 100x100", test_state()->last_bounds().ToString());

  // Drag up.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -800), 0);
  EXPECT_EQ("200,0 100x100", test_state()->last_bounds().ToString());
}

}  // namespace wm
}  // namespace ash
