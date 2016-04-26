// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include "ash/shell.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {

// Returns the window immediately below |window| in the current container.
aura::Window* GetWindowBelow(aura::Window* window) {
  aura::Window* parent = window->parent();
  if (!parent)
    return NULL;
  aura::Window::Windows::const_iterator iter =
      std::find(parent->children().begin(), parent->children().end(), window);
  CHECK(*iter == window);
  if (iter != parent->children().begin())
    return *(iter - 1);
  else
    return NULL;
}

// This class restores and moves a window to the front of the stacking order for
// the duration of the class's scope.
class ScopedShowWindow : public aura::WindowObserver {
 public:
  ScopedShowWindow();
  ~ScopedShowWindow() override;

  // Show |window| at the top of the stacking order.
  void Show(aura::Window* window);

  // Cancel restoring the window on going out of scope.
  void CancelRestore();

 private:
  // aura::WindowObserver:
  void OnWillRemoveWindow(aura::Window* window) override;

  // The window being shown.
  aura::Window* window_;

  // The window immediately below where window_ belongs.
  aura::Window* stack_window_above_;

  // If true, minimize window_ on going out of scope.
  bool minimized_;

  DISALLOW_COPY_AND_ASSIGN(ScopedShowWindow);
};

ScopedShowWindow::ScopedShowWindow()
    : window_(NULL),
      stack_window_above_(NULL),
      minimized_(false) {
}

ScopedShowWindow::~ScopedShowWindow() {
  if (window_) {
    window_->parent()->RemoveObserver(this);

    // Restore window's stacking position.
    if (stack_window_above_)
      window_->parent()->StackChildAbove(window_, stack_window_above_);
    else
      window_->parent()->StackChildAtBottom(window_);

    // Restore minimized state.
    if (minimized_)
      wm::GetWindowState(window_)->Minimize();
  }
}

void ScopedShowWindow::Show(aura::Window* window) {
  DCHECK(!window_);
  window_ = window;
  stack_window_above_ = GetWindowBelow(window);
  minimized_ = wm::GetWindowState(window)->IsMinimized();
  window_->parent()->AddObserver(this);
  window_->Show();
  wm::GetWindowState(window_)->Activate();
}

void ScopedShowWindow::CancelRestore() {
  if (!window_)
    return;
  window_->parent()->RemoveObserver(this);
  window_ = stack_window_above_ = NULL;
}

void ScopedShowWindow::OnWillRemoveWindow(aura::Window* window) {
  if (window == window_) {
    CancelRestore();
  } else if (window == stack_window_above_) {
    // If the window this window was above is removed, use the next window down
    // as the restore marker.
    stack_window_above_ = GetWindowBelow(stack_window_above_);
  }
}

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows),
      current_index_(0) {
  ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(true);

  for (auto* window : windows_)
    window->AddObserver(this);
}

WindowCycleList::~WindowCycleList() {
  ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(false);
  for (auto* window : windows_) {
    // TODO(oshima): Remove this once crbug.com/483491 is fixed.
    CHECK(window);
    window->RemoveObserver(this);
  }
  if (showing_window_)
    showing_window_->CancelRestore();
}

void WindowCycleList::Step(WindowCycleController::Direction direction) {
  if (windows_.empty())
    return;

  // When there is only one window, we should give feedback to the user. If the
  // window is minimized, we should also show it.
  if (windows_.size() == 1) {
    ::wm::AnimateWindow(windows_[0], ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    windows_[0]->Show();
    wm::GetWindowState(windows_[0])->Activate();
    return;
  }

  DCHECK(static_cast<size_t>(current_index_) < windows_.size());

  // We're in a valid cycle, so step forward or backward.
  current_index_ += direction == WindowCycleController::FORWARD ? 1 : -1;

  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);

  // Make sure the next window is visible.
  showing_window_.reset(new ScopedShowWindow);
  showing_window_->Show(windows_[current_index_]);
}

void WindowCycleList::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);

  WindowList::iterator i = std::find(windows_.begin(), windows_.end(), window);
  // TODO(oshima): Change this back to DCHECK once crbug.com/483491 is fixed.
  CHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index ||
      current_index_ == static_cast<int>(windows_.size())) {
    current_index_--;
  }
}

}  // namespace ash
