// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm_window.h"

#include <memory>

#include "ash/common/test/ash_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

using WmWindowTest = AshTest;

namespace {

// Tracks calls to OnWindowVisibilityChanged().
class VisibilityObserver : public aura::WindowObserver {
 public:
  // Attaches a aura::WindowObserver to |window_to_add_observer_to| and sets
  // |last_observed_window_| and |last_observed_visible_value_| to the values
  // of the last call to OnWindowVisibilityChanged().
  explicit VisibilityObserver(WmWindow* window_to_add_observer_to)
      : window_to_add_observer_to_(window_to_add_observer_to) {
    window_to_add_observer_to_->aura_window()->AddObserver(this);
  }
  ~VisibilityObserver() override {
    window_to_add_observer_to_->aura_window()->RemoveObserver(this);
  }

  // The values last supplied to OnWindowVisibilityChanged().
  WmWindow* last_observed_window() { return last_observed_window_; }
  bool last_observed_visible_value() const {
    return last_observed_visible_value_;
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    last_observed_window_ = WmWindow::Get(window);
    last_observed_visible_value_ = visible;
  }

 private:
  WmWindow* window_to_add_observer_to_;
  WmWindow* last_observed_window_ = nullptr;
  bool last_observed_visible_value_ = false;

  DISALLOW_COPY_AND_ASSIGN(VisibilityObserver);
};

}  // namespace

// Verifies OnWindowVisibilityChanged() is called on a aura::WindowObserver
// attached
// to the parent when the child window's visibility changes.
TEST_F(WmWindowTest, OnWindowVisibilityChangedCalledOnAncestor) {
  std::unique_ptr<WindowOwner> window_owner = CreateTestWindow();
  WmWindow* window = window_owner->window();
  std::unique_ptr<WindowOwner> child_owner =
      CreateChildWindow(window_owner->window());
  WmWindow* child_window = child_owner->window();
  VisibilityObserver observer(window);
  child_window->Hide();
  EXPECT_EQ(child_window, observer.last_observed_window());
  EXPECT_FALSE(observer.last_observed_visible_value());
}

// Verifies OnWindowVisibilityChanged() is called on a aura::WindowObserver
// attached
// to a child when the parent window's visibility changes.
TEST_F(WmWindowTest, OnWindowVisibilityChangedCalledOnChild) {
  std::unique_ptr<WindowOwner> parent_window_owner = CreateTestWindow();
  WmWindow* parent_window = parent_window_owner->window();
  std::unique_ptr<WindowOwner> child_owner = CreateChildWindow(parent_window);
  WmWindow* child_window = child_owner->window();
  VisibilityObserver observer(child_window);
  parent_window->Hide();
  EXPECT_EQ(parent_window, observer.last_observed_window());
  EXPECT_FALSE(observer.last_observed_visible_value());
}

}  // namespace ash
