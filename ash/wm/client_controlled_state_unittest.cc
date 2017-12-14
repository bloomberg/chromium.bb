// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {
namespace {

constexpr gfx::Rect kInitialBounds(0, 0, 100, 100);

class TestClientControlledStateDelegate
    : public ClientControlledState::Delegate {
 public:
  TestClientControlledStateDelegate() = default;
  ~TestClientControlledStateDelegate() override = default;

  void HandleWindowStateRequest(WindowState* window_state,
                                mojom::WindowStateType next_state) override {
    old_state_ = window_state->GetStateType();
    new_state_ = next_state;
  }

  void HandleBoundsRequest(WindowState* window_state,
                           const gfx::Rect& bounds) override {
    requested_bounds_ = bounds;
  }

  mojom::WindowStateType old_state() const { return old_state_; }

  mojom::WindowStateType new_state() const { return new_state_; }

  const gfx::Rect& requested_bounds() const { return requested_bounds_; }

  void reset() {
    old_state_ = mojom::WindowStateType::DEFAULT;
    new_state_ = mojom::WindowStateType::DEFAULT;
    requested_bounds_.SetRect(0, 0, 0, 0);
  }

 private:
  mojom::WindowStateType old_state_ = mojom::WindowStateType::DEFAULT;
  mojom::WindowStateType new_state_ = mojom::WindowStateType::DEFAULT;
  gfx::Rect requested_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestClientControlledStateDelegate);
};

}  // namespace

class ClientControlledStateTest : public AshTestBase {
 public:
  ClientControlledStateTest() = default;
  ~ClientControlledStateTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        kShellWindowId_DefaultContainer);
    params.bounds = kInitialBounds;
    widget_->Init(params);
    wm::WindowState* window_state = wm::GetWindowState(window());
    auto delegate = std::make_unique<TestClientControlledStateDelegate>();
    delegate_ = delegate.get();
    auto state = std::make_unique<ClientControlledState>(std::move(delegate));
    state_ = state.get();
    window_state->SetStateObject(std::move(state));
    widget_->Show();
  }

  void TearDown() override {
    widget_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  aura::Window* window() { return widget_->GetNativeWindow(); }
  WindowState* window_state() { return GetWindowState(window()); }
  ClientControlledState* state() { return state_; }
  TestClientControlledStateDelegate* delegate() { return delegate_; }
  views::Widget* widget() { return widget_.get(); }
  ScreenPinningController* GetScreenPinningController() {
    return Shell::Get()->screen_pinning_controller();
  }

 private:
  ClientControlledState* state_ = nullptr;
  TestClientControlledStateDelegate* delegate_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledStateTest);
};

// Make sure that calling Maximize()/Minimize()/Fullscreen() result in
// sending the state change request and won't change the state immediately.
// The state will be updated when ClientControlledState::EnterToNextState
// is called.
TEST_F(ClientControlledStateTest, Maximize) {
  widget()->Maximize();
  // The state shouldn't be updated until EnterToNextState is called.
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->new_state());
  // Now enters the new state.
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMaximized());
  // Bounds is controlled by client.
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, Minimize) {
  widget()->Minimize();
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  // use wm::Unminimize to unminimize.
  widget()->Minimize();
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  ::wm::Unminimize(widget()->GetNativeWindow());
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(ui::SHOW_STATE_NORMAL,
            widget()->GetNativeWindow()->GetProperty(
                aura::client::kPreMinimizedShowStateKey));
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, Fullscreen) {
  widget()->SetFullscreen(true);
  EXPECT_FALSE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::FULLSCREEN, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(false);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(mojom::WindowStateType::FULLSCREEN, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_FALSE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

// Make sure toggle fullscreen from maximized state goes back to
// maximized state.
TEST_F(ClientControlledStateTest, MaximizeToFullscreen) {
  widget()->Maximize();
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(true);
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::FULLSCREEN, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(false);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(mojom::WindowStateType::FULLSCREEN, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(mojom::WindowStateType::MAXIMIZED, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, IgnoreWorkspace) {
  widget()->Maximize();
  state()->EnterNextState(window_state(), delegate()->new_state(),
                          ClientControlledState::kAnimationNone);
  EXPECT_TRUE(widget()->IsMaximized());
  delegate()->reset();

  UpdateDisplay("1000x800");

  // Client is responsible to handle workspace change, so
  // no action should be taken.
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->new_state());
  EXPECT_EQ(gfx::Rect(), delegate()->requested_bounds());
}

TEST_F(ClientControlledStateTest, SetBounds) {
  constexpr gfx::Rect new_bounds(100, 100, 100, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(new_bounds, delegate()->requested_bounds());
  state()->set_bounds_locally(true);
  widget()->SetBounds(delegate()->requested_bounds());
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());
}

// Pin events should be applied immediately.
TEST_F(ClientControlledStateTest, Pinned) {
  ASSERT_FALSE(window_state()->IsPinned());
  ASSERT_FALSE(GetScreenPinningController()->IsPinned());

  const WMEvent pin_event(WM_EVENT_PIN);
  window_state()->OnWMEvent(&pin_event);
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_EQ(mojom::WindowStateType::PINNED, window_state()->GetStateType());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::PINNED, delegate()->new_state());

  // All state transition events are ignored except for NORMAL.
  widget()->Maximize();
  EXPECT_EQ(mojom::WindowStateType::PINNED, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  widget()->Minimize();
  EXPECT_EQ(mojom::WindowStateType::PINNED, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_TRUE(window()->IsVisible());

  widget()->SetFullscreen(true);
  EXPECT_EQ(mojom::WindowStateType::PINNED, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // WM/User cannot change the bounds of the pinned window.
  constexpr gfx::Rect new_bounds(100, 100, 200, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());
  // But client can change the bounds of the pinned window.
  state()->set_bounds_locally(true);
  widget()->SetBounds(new_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, window_state()->GetStateType());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  // Two windows cannot be pinned simultaneously.
  auto widget2 = CreateTestWidget();
  WindowState* window_state_2 =
      ::ash::wm::GetWindowState(widget2->GetNativeWindow());
  window_state_2->OnWMEvent(&pin_event);
  EXPECT_TRUE(window_state_2->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // Pin request should fail.
  window_state()->OnWMEvent(&pin_event);
  EXPECT_FALSE(window_state()->IsPinned());
}

TEST_F(ClientControlledStateTest, TrustedPinnedBasic) {
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  const WMEvent trusted_pin_event(WM_EVENT_TRUSTED_PIN);
  window_state()->OnWMEvent(&trusted_pin_event);
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  EXPECT_EQ(mojom::WindowStateType::TRUSTED_PINNED,
            window_state()->GetStateType());
  EXPECT_EQ(mojom::WindowStateType::DEFAULT, delegate()->old_state());
  EXPECT_EQ(mojom::WindowStateType::TRUSTED_PINNED, delegate()->new_state());

  // All state transition events are ignored except for NORMAL.
  widget()->Maximize();
  EXPECT_EQ(mojom::WindowStateType::TRUSTED_PINNED,
            window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  widget()->Minimize();
  EXPECT_EQ(mojom::WindowStateType::TRUSTED_PINNED,
            window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_TRUE(window()->IsVisible());

  widget()->SetFullscreen(true);
  EXPECT_EQ(mojom::WindowStateType::TRUSTED_PINNED,
            window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // WM/User cannot change the bounds of the trusted-pinned window.
  constexpr gfx::Rect new_bounds(100, 100, 200, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());
  // But client can change the bounds of the trusted-pinned window.
  state()->set_bounds_locally(true);
  widget()->SetBounds(new_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, window_state()->GetStateType());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  // Two windows cannot be trusted-pinned simultaneously.
  auto widget2 = CreateTestWidget();
  WindowState* window_state_2 =
      ::ash::wm::GetWindowState(widget2->GetNativeWindow());
  window_state_2->OnWMEvent(&trusted_pin_event);
  EXPECT_TRUE(window_state_2->IsTrustedPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  EXPECT_FALSE(window_state()->IsTrustedPinned());
  window_state()->OnWMEvent(&trusted_pin_event);
  EXPECT_FALSE(window_state()->IsTrustedPinned());
  EXPECT_TRUE(window_state_2->IsTrustedPinned());
}

}  // namespace wm
}  // namespace ash
