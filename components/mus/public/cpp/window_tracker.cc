// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_tracker.h"

namespace mus {

WindowTracker::WindowTracker() {}

WindowTracker::~WindowTracker() {
  for (Windows::iterator i = windows_.begin(); i != windows_.end(); ++i)
    (*i)->RemoveObserver(this);
}

void WindowTracker::Add(Window* window) {
  if (windows_.count(window))
    return;

  window->AddObserver(this);
  windows_.insert(window);
}

void WindowTracker::Remove(Window* window) {
  if (windows_.count(window)) {
    windows_.erase(window);
    window->RemoveObserver(this);
  }
}

bool WindowTracker::Contains(Window* window) {
  return windows_.count(window) > 0;
}

void WindowTracker::OnWindowDestroying(Window* window) {
  DCHECK_GT(windows_.count(window), 0u);
  Remove(window);
}

}  // namespace mus
