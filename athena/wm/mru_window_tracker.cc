// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/mru_window_tracker.h"

#include "ui/aura/window.h"

namespace athena {

MruWindowTracker::MruWindowTracker(aura::Window* container)
    : container_(container) {
  container->AddObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  if (container_)
    container_->RemoveObserver(this);
}

aura::Window::Windows MruWindowTracker::GetWindowList() const {
  return aura::Window::Windows(mru_windows_.begin(), mru_windows_.end());
}

void MruWindowTracker::MoveToFront(aura::Window* window) {
  DCHECK(window);
  CHECK_EQ(container_, window->parent());
  std::list<aura::Window*>::iterator it =
      std::find(mru_windows_.begin(), mru_windows_.end(), window);
  DCHECK(it != mru_windows_.end());
  mru_windows_.erase(it);
  mru_windows_.push_back(window);
}

// Overridden from WindowObserver:
void MruWindowTracker::OnWillRemoveWindow(aura::Window* window) {
  std::list<aura::Window*>::iterator it =
      std::find(mru_windows_.begin(), mru_windows_.end(), window);
  if (it == mru_windows_.end()) {
    // All normal windows should be tracked in mru_windows_
    DCHECK_NE(window->type(), ui::wm::WINDOW_TYPE_NORMAL);
    return;
  }
  mru_windows_.erase(it);
}

void MruWindowTracker::OnWindowAdded(aura::Window* new_window) {
  // We are only interested in ordering normal windows.
  if (new_window->type() == ui::wm::WINDOW_TYPE_NORMAL)
    mru_windows_.push_back(new_window);
}

}  // namespace athena
