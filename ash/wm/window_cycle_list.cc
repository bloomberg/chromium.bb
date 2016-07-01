// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"

namespace ash {

// Returns the window immediately below |window| in the current container.
WmWindow* GetWindowBelow(WmWindow* window) {
  WmWindow* parent = window->GetParent();
  if (!parent)
    return nullptr;
  const WmWindow::Windows children = parent->GetChildren();
  auto iter = std::find(children.begin(), children.end(), window);
  CHECK(*iter == window);
  return (iter != children.begin()) ? *(iter - 1) : nullptr;
}

// This class restores and moves a window to the front of the stacking order for
// the duration of the class's scope.
class ScopedShowWindow : public WmWindowObserver {
 public:
  ScopedShowWindow();
  ~ScopedShowWindow() override;

  // Show |window| at the top of the stacking order.
  void Show(WmWindow* window);

  // Cancel restoring the window on going out of scope.
  void CancelRestore();

 private:
  // WmWindowObserver:
  void OnWindowTreeChanging(WmWindow* window,
                            const TreeChangeParams& params) override;

  // The window being shown.
  WmWindow* window_;

  // The window immediately below where window_ belongs.
  WmWindow* stack_window_above_;

  // If true, minimize window_ on going out of scope.
  bool minimized_;

  DISALLOW_COPY_AND_ASSIGN(ScopedShowWindow);
};

ScopedShowWindow::ScopedShowWindow()
    : window_(nullptr), stack_window_above_(nullptr), minimized_(false) {}

ScopedShowWindow::~ScopedShowWindow() {
  if (window_) {
    window_->GetParent()->RemoveObserver(this);

    // Restore window's stacking position.
    if (stack_window_above_)
      window_->GetParent()->StackChildAbove(window_, stack_window_above_);
    else
      window_->GetParent()->StackChildAtBottom(window_);

    // Restore minimized state.
    if (minimized_)
      window_->GetWindowState()->Minimize();
  }
}

void ScopedShowWindow::Show(WmWindow* window) {
  DCHECK(!window_);
  window_ = window;
  stack_window_above_ = GetWindowBelow(window);
  minimized_ = window->GetWindowState()->IsMinimized();
  window_->GetParent()->AddObserver(this);
  window_->Show();
  window_->GetWindowState()->Activate();
}

void ScopedShowWindow::CancelRestore() {
  if (!window_)
    return;
  window_->GetParent()->RemoveObserver(this);
  window_ = stack_window_above_ = nullptr;
}

void ScopedShowWindow::OnWindowTreeChanging(WmWindow* window,
                                            const TreeChangeParams& params) {
  // Only interested in removal.
  if (params.new_parent != nullptr)
    return;

  if (params.target == window_) {
    CancelRestore();
  } else if (params.target == stack_window_above_) {
    // If the window this window was above is removed, use the next window down
    // as the restore marker.
    stack_window_above_ = GetWindowBelow(stack_window_above_);
  }
}

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows), current_index_(0) {
  WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  for (WmWindow* window : windows_)
    window->AddObserver(this);
}

WindowCycleList::~WindowCycleList() {
  WmShell::Get()->mru_window_tracker()->SetIgnoreActivations(false);
  for (WmWindow* window : windows_) {
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
    windows_[0]->Animate(::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    windows_[0]->Show();
    windows_[0]->GetWindowState()->Activate();
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

void WindowCycleList::OnWindowDestroying(WmWindow* window) {
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
