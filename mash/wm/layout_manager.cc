// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/layout_manager.h"

#include <stdint.h>

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"

namespace mash {
namespace wm {

LayoutManager::~LayoutManager() {
  Uninstall();
}

LayoutManager::LayoutManager(mus::Window* owner) : owner_(owner) {
  owner_->AddObserver(this);
  DCHECK(owner->children().empty());
}

void LayoutManager::Uninstall() {
  if (!owner_)
    return;
  owner_->RemoveObserver(this);
  for (auto child : owner_->children())
    child->RemoveObserver(this);
  owner_ = nullptr;
}

void LayoutManager::OnTreeChanged(
    const mus::WindowObserver::TreeChangeParams& params) {
  DCHECK(params.target);
  if (params.new_parent == owner_) {
    // params.target was added to the layout.
    WindowAdded(params.target);
    params.target->AddObserver(this);
    LayoutWindow(params.target);
  } else if (params.old_parent == owner_) {
    // params.target was removed from the layout.
    params.target->RemoveObserver(this);
    WindowRemoved(params.target);
  }
}

void LayoutManager::OnWindowDestroying(mus::Window* window) {
  if (owner_ == window)
    Uninstall();
}

void LayoutManager::OnWindowBoundsChanged(mus::Window* window,
                                          const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  if (window != owner_)
    return;

  // Changes to the container's bounds require all windows to be laid out.
  for (auto child : window->children())
    LayoutWindow(child);
}

void LayoutManager::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (window == owner_)
    return;

  // Changes to the following properties require the window to be laid out.
  if (layout_properties_.count(name) > 0)
    LayoutWindow(window);
}

void LayoutManager::WindowAdded(mus::Window* window) {}
void LayoutManager::WindowRemoved(mus::Window* window) {}

void LayoutManager::AddLayoutProperty(const std::string& name) {
  layout_properties_.insert(name);
}

}  // namespace wm
}  // namespace mash
