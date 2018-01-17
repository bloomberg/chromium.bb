// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sort_windows_by_z_index.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"

namespace ui {

namespace {

// Append windows in |windows| that are descendant of |root_window| to
// |sorted_windows| in z-order, from topmost to bottommost.
void AppendDescendantsSortedByZIndex(
    const aura::Window* root_window,
    const base::flat_set<aura::Window*>& windows,
    std::vector<aura::Window*>* sorted_windows) {
  const aura::Window::Windows& children = root_window->children();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    aura::Window* window = *it;
    if (base::ContainsKey(windows, window)) {
      sorted_windows->push_back(window);
      // Skip children of |window| since a window in |windows| is not expected
      // to be the parent of another window in |windows|.
    } else {
      AppendDescendantsSortedByZIndex(window, windows, sorted_windows);
    }
  }
}

void DoSortWindowsByZIndex(std::unique_ptr<aura::WindowTracker> window_tracker,
                           SortWindowsByZIndexCallback callback) {
  const base::flat_set<aura::Window*> windows(window_tracker->windows(),
                                              base::KEEP_FIRST_OF_DUPES);
  std::vector<aura::Window*> sorted_windows;
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
    AppendDescendantsSortedByZIndex(root_window, windows, &sorted_windows);
  DCHECK_EQ(windows.size(), sorted_windows.size());
  std::move(callback).Run(sorted_windows);
}

}  // namespace

void SortWindowsByZIndex(const std::vector<aura::Window*>& windows,
                         SortWindowsByZIndexCallback callback) {
  auto window_tracker = std::make_unique<aura::WindowTracker>();
  for (aura::Window* window : windows)
    window_tracker->Add(window);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DoSortWindowsByZIndex, std::move(window_tracker),
                     std::move(callback)));
}

}  // namespace ui
