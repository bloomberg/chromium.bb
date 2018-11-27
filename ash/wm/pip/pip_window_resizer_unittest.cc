// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"

#include <string>
#include <utility>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/public/keyboard_switches.h"
#include "ui/keyboard/test/keyboard_test_util.h"
#include "ui/views/widget/widget.h"

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
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();
    SetTouchKeyboardEnabled(true);

    widget_ = CreateWidgetForTest(gfx::Rect(200, 200, 100, 100));
    window_ = widget_->GetNativeWindow();
    window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
    test_state_ = new FakeWindowState(mojom::WindowStateType::PIP);
    wm::GetWindowState(window_)->SetStateObject(
        std::unique_ptr<wm::WindowState::State>(test_state_));
  }

  void TearDown() override {
    SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

 protected:
  aura::Window* window() { return window_; }
  FakeWindowState* test_state() { return test_state_; }

  std::unique_ptr<views::Widget> CreateWidgetForTest(const gfx::Rect& bounds) {
    return CreateTestWidget(nullptr, kShellWindowId_AlwaysOnTopContainer,
                            bounds);
  }

  PipWindowResizer* CreateResizerForTest(int window_component) {
    return CreateResizerForTest(window_component, window());
  }

  PipWindowResizer* CreateResizerForTest(int window_component,
                                         aura::Window* window) {
    wm::WindowState* window_state = wm::GetWindowState(window);
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

  void Fling(std::unique_ptr<WindowResizer> resizer,
             float velocity_x,
             float velocity_y) {
    aura::Window* target_window = resizer->GetTarget();
    base::TimeTicks timestamp = base::TimeTicks::Now();
    ui::GestureEventDetails details = ui::GestureEventDetails(
        ui::ET_SCROLL_FLING_START, velocity_x, velocity_y);
    ui::GestureEvent event = ui::GestureEvent(
        target_window->bounds().origin().x(),
        target_window->bounds().origin().y(), ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(target_window);
    resizer->FlingOrSwipe(&event);
  }

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    aura::Window* root = Shell::GetPrimaryRootWindow();
    Shell::Get()->SetDisplayWorkAreaInsets(root, gfx::Insets());
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  aura::Window* window_;
  FakeWindowState* test_state_;

  DISALLOW_COPY_AND_ASSIGN(PipWindowResizerTest);
};

TEST_F(PipWindowResizerTest, PipWindowCanDrag) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanResize) {
  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowDragIsRestrictedToWorkArea) {
  UpdateWorkArea("400x400");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 800, 0), 0);
  EXPECT_EQ(gfx::Rect(292, 200, 100, 100), test_state()->last_bounds());

  // Drag down.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 800), 0);
  EXPECT_EQ(gfx::Rect(200, 292, 100, 100), test_state()->last_bounds());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -800, 0), 0);
  EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_bounds());

  // Drag up.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -800), 0);
  EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanBeDraggedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanBeResizedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  UpdateWorkArea("400x800");
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110), test_state()->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowCanBeSwipeDismissed) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -100, 0), 0);

  // Should be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_TRUE(widget->IsClosed());
}

TEST_F(PipWindowResizerTest, PipWindowPartiallySwipedDoesNotDismiss) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Drag to the left, but only a little bit.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 0), 0);

  // Should not be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_FALSE(widget->IsClosed());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowInSwipeToDismissGestureLocksToAxis) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Drag to the left, but only a little bit, to start a swipe-to-dismiss.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 0), 0);
  EXPECT_EQ(gfx::Rect(-22, 8, 100, 100), test_state->last_bounds());

  // Now try to drag down, it should be locked to the horizontal axis.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 30), 0);
  EXPECT_EQ(gfx::Rect(-22, 8, 100, 100), test_state->last_bounds());
}

TEST_F(PipWindowResizerTest,
       PipWindowMovedAwayFromScreenEdgeNoLongerCanSwipeToDismiss) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 16, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Drag to the right and up a bit.
  resizer->Drag(CalculateDragPoint(*resizer, 30, -8), 0);
  EXPECT_EQ(gfx::Rect(38, 8, 100, 100), test_state->last_bounds());

  // Now try to drag to the left start a swipe-to-dismiss. It should stop
  // at the edge of the work area.
  resizer->Drag(CalculateDragPoint(*resizer, -30, -8), 0);
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state->last_bounds());
}

TEST_F(PipWindowResizerTest, PipWindowAtCornerLocksToOneAxisOnSwipeToDismiss) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Try dragging up and to the left. It should lock onto the axis with the
  // largest displacement.
  resizer->Drag(CalculateDragPoint(*resizer, -30, -40), 0);
  EXPECT_EQ(gfx::Rect(8, -32, 100, 100), test_state->last_bounds());
}

TEST_F(
    PipWindowResizerTest,
    PipWindowMustBeDraggedMostlyInDirectionOfDismissToInitiateSwipeToDismiss) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Try a lot downward and a bit to the left. Swiping should not be initiated.
  resizer->Drag(CalculateDragPoint(*resizer, -30, 50), 0);
  EXPECT_EQ(gfx::Rect(8, 58, 100, 100), test_state->last_bounds());
}

TEST_F(PipWindowResizerTest,
       PipWindowDoesNotMoveUntilStatusOfSwipeToDismissGestureIsKnown) {
  UpdateWorkArea("400x400");
  auto widget = CreateWidgetForTest(gfx::Rect(8, 8, 100, 100));
  auto* window = widget->GetNativeWindow();
  FakeWindowState* test_state =
      new FakeWindowState(mojom::WindowStateType::PIP);
  wm::GetWindowState(window)->SetStateObject(
      std::unique_ptr<wm::WindowState::State>(test_state));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, window));
  ASSERT_TRUE(resizer.get());

  // Move a small amount - this should not trigger any bounds change, since
  // we don't know whether a swipe will start or not.
  resizer->Drag(CalculateDragPoint(*resizer, -4, 0), 0);
  EXPECT_TRUE(test_state->last_bounds().IsEmpty());
}

TEST_F(PipWindowResizerTest, PipWindowIsFlungToEdge) {
  UpdateWorkArea("400x400");

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    Fling(std::move(resizer), 0.f, 4000.f);

    // Flung downwards.
    EXPECT_EQ(gfx::Rect(200, 292, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, -10), 0);
    Fling(std::move(resizer), 0.f, -4000.f);

    // Flung upwards.
    EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
    Fling(std::move(resizer), 4000.f, 0.f);

    // Flung to the right.
    EXPECT_EQ(gfx::Rect(292, 200, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
    Fling(std::move(resizer), -4000.f, 0.f);

    // Flung to the left.
    EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_bounds());
  }
}

TEST_F(PipWindowResizerTest, PipWindowIsFlungDiagonally) {
  UpdateWorkArea("400x400");

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    Fling(std::move(resizer), 3000.f, 3000.f);

    // Flung downward and to the right, into the corner.
    EXPECT_EQ(gfx::Rect(292, 292, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, 4), 0);
    Fling(std::move(resizer), 3000.f, 4000.f);

    // Flung downward and to the right, but reaching the bottom edge first.
    EXPECT_EQ(gfx::Rect(269, 292, 100, 100), test_state()->last_bounds());
  }

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 4, 3), 0);
    Fling(std::move(resizer), 4000.f, 3000.f);

    // Flung downward and to the right, but reaching the right edge first.
    EXPECT_EQ(gfx::Rect(292, 269, 100, 100), test_state()->last_bounds());
  }
}

TEST_F(PipWindowResizerTest, PipWindowFlungAvoidsFloatingKeyboard) {
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  keyboard_controller->SetContainerType(
      keyboard::mojom::ContainerType::kFloating, gfx::Rect(0, 0, 1, 1),
      base::DoNothing());
  keyboard_controller->ShowKeyboard(/*lock=*/true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(8, 150, 100, 100));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Fling to the left - but don't intersect with the floating keyboard.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
  Fling(std::move(resizer), -4000.f, 0.f);

  // Appear below the keyboard.
  EXPECT_EQ(gfx::Rect(8, 258, 100, 100), test_state()->last_bounds());
}

}  // namespace wm
}  // namespace ash
