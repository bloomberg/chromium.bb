// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_WINDOW_TRACKER_H_
#define COMPONENTS_MUS_COMMON_WINDOW_TRACKER_H_

#include <vector>

#include "base/stl_util.h"
#include "mojo/public/cpp/system/macros.h"

namespace mus {

template <class T, class TObserver>
class WindowTrackerTemplate : public TObserver {
 public:
  // A vector<> is used for tracking the windows (instead of a set<>) because
  // the user may want to know about the order of the windows that have been
  // added.
  using WindowList = std::vector<T*>;

  WindowTrackerTemplate() {}
  ~WindowTrackerTemplate() override {
    for (T* window : windows_)
      window->RemoveObserver(this);
  }

  // Returns the set of windows being observed.
  const WindowList& windows() const { return windows_; }

  // Adds |window| to the set of Windows being tracked.
  void Add(T* window) {
    if (ContainsValue(windows_, window))
      return;

    window->AddObserver(this);
    windows_.push_back(window);
  }

  // Removes |window| from the set of windows being tracked.
  void Remove(T* window) {
    auto iter = std::find(windows_.begin(), windows_.end(), window);
    if (iter != windows_.end()) {
      window->RemoveObserver(this);
      windows_.erase(iter);
    }
  }

  // Returns true if |window| was previously added and has not been removed or
  // deleted.
  bool Contains(T* window) const { return ContainsValue(windows_, window); }

  // Observer overrides:
  void OnWindowDestroying(T* window) override {
    DCHECK(Contains(window));
    Remove(window);
  }

 private:
  WindowList windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowTrackerTemplate);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_WINDOW_TRACKER_H_
