// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/mus_layout_manager_adapter.h"

#include "ash/wm/common/wm_layout_manager.h"
#include "components/mus/public/cpp/window.h"
#include "mash/wm/bridge/wm_window_mus.h"

namespace mash {
namespace wm {

MusLayoutManagerAdapter::ChildWindowObserver::ChildWindowObserver(
    MusLayoutManagerAdapter* adapter)
    : adapter_(adapter) {}

MusLayoutManagerAdapter::ChildWindowObserver::~ChildWindowObserver() {}

void MusLayoutManagerAdapter::ChildWindowObserver::OnWindowVisibilityChanged(
    mus::Window* window) {
  adapter_->layout_manager_->OnChildWindowVisibilityChanged(
      WmWindowMus::Get(window), window->visible());
}

MusLayoutManagerAdapter::MusLayoutManagerAdapter(
    mus::Window* window,
    std::unique_ptr<ash::wm::WmLayoutManager> layout_manager)
    : window_(window),
      child_window_observer_(this),
      layout_manager_(std::move(layout_manager)) {
  window_->AddObserver(this);
  for (mus::Window* child : window_->children())
    child->AddObserver(&child_window_observer_);
}

MusLayoutManagerAdapter::~MusLayoutManagerAdapter() {
  for (mus::Window* child : window_->children())
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
  } else if (params.old_parent == window_) {
    params.target->RemoveObserver(&child_window_observer_);
    layout_manager_->OnWindowRemovedFromLayout(WmWindowMus::Get(params.target));
  }
}

void MusLayoutManagerAdapter::OnWindowBoundsChanged(
    mus::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  layout_manager_->OnWindowResized();
}

}  // namespace wm
}  // namespace mash
