// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/mus_layout_manager_adapter.h"

#include "ash/common/wm_layout_manager.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "services/ui/public/cpp/window.h"

namespace ash {
namespace mus {

MusLayoutManagerAdapter::ChildWindowObserver::ChildWindowObserver(
    MusLayoutManagerAdapter* adapter)
    : adapter_(adapter) {}

MusLayoutManagerAdapter::ChildWindowObserver::~ChildWindowObserver() {}

void MusLayoutManagerAdapter::ChildWindowObserver::OnWindowVisibilityChanged(
    ::ui::Window* window) {
  adapter_->layout_manager_->OnChildWindowVisibilityChanged(
      WmWindowMus::Get(window), window->visible());
}

MusLayoutManagerAdapter::MusLayoutManagerAdapter(
    ::ui::Window* window,
    std::unique_ptr<WmLayoutManager> layout_manager)
    : window_(window),
      child_window_observer_(this),
      layout_manager_(std::move(layout_manager)) {
  window_->AddObserver(this);
  for (::ui::Window* child : window_->children())
    child->AddObserver(&child_window_observer_);
}

MusLayoutManagerAdapter::~MusLayoutManagerAdapter() {
  for (::ui::Window* child : window_->children())
    child->RemoveObserver(&child_window_observer_);

  window_->RemoveObserver(this);
}

void MusLayoutManagerAdapter::OnTreeChanging(const TreeChangeParams& params) {
  if (params.old_parent == window_) {
    layout_manager_->OnWillRemoveWindowFromLayout(
        WmWindowMus::Get(params.target));
  }
}

void MusLayoutManagerAdapter::OnTreeChanged(const TreeChangeParams& params) {
  if (params.new_parent == window_) {
    layout_manager_->OnWindowAddedToLayout(WmWindowMus::Get(params.target));
    params.target->AddObserver(&child_window_observer_);
  } else if (params.old_parent == window_) {
    params.target->RemoveObserver(&child_window_observer_);
    layout_manager_->OnWindowRemovedFromLayout(WmWindowMus::Get(params.target));
  }
}

void MusLayoutManagerAdapter::OnWindowBoundsChanged(
    ::ui::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  layout_manager_->OnWindowResized();
}

}  // namespace mus
}  // namespace ash
