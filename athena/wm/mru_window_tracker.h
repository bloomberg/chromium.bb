// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_MRU_WINDOW_TRACKER_H_
#define ATHENA_WM_MRU_WINDOW_TRACKER_H_

#include <list>

#include "athena/wm/public/window_list_provider.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace athena {

// Maintains a most recently used list of windows. This is used for window
// cycling and overview mode.
class MruWindowTracker : public WindowListProvider,
                         public aura::WindowObserver {
 public:
  explicit MruWindowTracker(aura::Window* container);
  virtual ~MruWindowTracker();

  // Overridden from WindowListProvider
  virtual aura::Window::Windows GetWindowList() const OVERRIDE;

  // Updates the mru_windows_ list to move |window| to the front.
  // |window| must be the child of |container_|.
  virtual void MoveToFront(aura::Window* window) OVERRIDE;

 private:
  // Overridden from WindowObserver:
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;

  // List of windows that have been used in the container, sorted by most
  // recently used.
  std::list<aura::Window*> mru_windows_;
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(MruWindowTracker);
};

}  // namespace athena

#endif  // ATHENA_WM_MRU_WINDOW_TRACKER_H_
