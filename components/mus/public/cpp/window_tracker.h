// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_

#include <stdint.h>
#include <set>

#include "components/mus/public/cpp/window_observer.h"
#include "third_party/mojo/src/mojo/public/cpp/system/macros.h"

namespace mus {

class WindowTracker : public WindowObserver {
 public:
  using Windows = std::set<Window*>;

  WindowTracker();
  ~WindowTracker() override;

  // Returns the set of windows being observed.
  const std::set<Window*>& windows() const { return windows_; }

  // Adds |window| to the set of Windows being tracked.
  void Add(Window* window);

  // Removes |window| from the set of windows being tracked.
  void Remove(Window* window);

  // Returns true if |window| was previously added and has not been removed or
  // deleted.
  bool Contains(Window* window);

  // WindowObserver overrides:
  void OnWindowDestroying(Window* window) override;

 private:
  Windows windows_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_
