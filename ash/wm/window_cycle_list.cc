// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/window_animations.h"

namespace ash {

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows),
      current_index_(-1) {
  ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(true);
  // Locate the currently active window in the list to use as our start point.
  aura::Window* active_window = wm::GetActiveWindow();

  // The active window may not be in the cycle list, which is expected if there
  // are additional modal windows on the screen.
  current_index_ = GetWindowIndex(active_window);

  for (WindowList::const_iterator i = windows_.begin(); i != windows_.end();
       ++i) {
    (*i)->AddObserver(this);
  }
}

WindowCycleList::~WindowCycleList() {
  ash::Shell::GetInstance()->mru_window_tracker()->SetIgnoreActivations(false);
  for (WindowList::const_iterator i = windows_.begin(); i != windows_.end();
       ++i) {
    (*i)->RemoveObserver(this);
  }
}

void WindowCycleList::Step(Direction direction) {
  if (windows_.empty())
    return;

  if (current_index_ == -1) {
    // We weren't able to find our active window in the shell delegate's
    // provided window list.  Just switch to the first (or last) one.
    current_index_ = (direction == FORWARD ? 0 : windows_.size() - 1);
  } else {
    // When there is only one window, we should give a feedback to user.
    if (windows_.size() == 1) {
      AnimateWindow(windows_[0],
                    views::corewm::WINDOW_ANIMATION_TYPE_BOUNCE);
      return;
    }
    // We're in a valid cycle, so step forward or backward.
    current_index_ += (direction == FORWARD ? 1 : -1);
  }
  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);
  // Make sure the next window is visible.
  windows_[current_index_]->Show();
  wm::ActivateWindow(windows_[current_index_]);
}

int WindowCycleList::GetWindowIndex(aura::Window* window) {
  WindowList::const_iterator it =
      std::find(windows_.begin(), windows_.end(), window);
  if (it == windows_.end())
    return -1;  // Not found.
  return it - windows_.begin();
}

void WindowCycleList::OnWindowDestroyed(aura::Window* window) {
  WindowList::iterator i = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index)
    current_index_--;
  else if (current_index_ == static_cast<int>(windows_.size()))
    current_index_--;
}

}  // namespace ash
