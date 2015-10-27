// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_layout.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"

namespace {

mus::mojom::ShowState GetWindowShowState(mus::Window* window) {
  if (window->HasSharedProperty(
      mus::mojom::WindowManager::kShowState_Property)) {
    return static_cast<mus::mojom::ShowState>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kShowState_Property));
  }
  return mus::mojom::SHOW_STATE_RESTORED;
}

mojo::Rect GetWindowUserSetBounds(mus::Window* window) {
  if (window->HasSharedProperty(
      mus::mojom::WindowManager::kUserSetBounds_Property)) {
    return window->GetSharedProperty<mojo::Rect>(
        mus::mojom::WindowManager::kUserSetBounds_Property);
  }
  return mojo::Rect();
}

mojo::Size GetWindowPreferredSize(mus::Window* window) {
  if (window->HasSharedProperty(
      mus::mojom::WindowManager::kPreferredSize_Property)) {
    return window->GetSharedProperty<mojo::Size>(
        mus::mojom::WindowManager::kPreferredSize_Property);
  }
  return mojo::Size();
}

bool IsRectEmpty(const mojo::Rect& rect) {
  return rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0;
}

bool IsSizeEmpty(const mojo::Size& size) {
  return size.width == 0 && size.height == 0;
}

}  // namespace

WindowLayout::WindowLayout(mus::Window* owner) : owner_(owner) {
  owner_->AddObserver(this);
  for (auto child : owner_->children())
    child->AddObserver(this);
}
WindowLayout::~WindowLayout() {
  owner_->RemoveObserver(this);
  for (auto child : owner_->children())
    child->RemoveObserver(this);
}

void WindowLayout::OnTreeChanged(
    const mus::WindowObserver::TreeChangeParams& params) {
  DCHECK(params.target);
  if (params.new_parent == owner_) {
    // params.target was added to the layout.
    params.target->AddObserver(this);
    LayoutWindow(params.target);
  } else if (params.old_parent == owner_) {
    // params.target was removed from the layout.
    params.target->RemoveObserver(this);
  }
}

void WindowLayout::OnWindowBoundsChanged(
    mus::Window* window,
    const mojo::Rect& old_bounds,
    const mojo::Rect& new_bounds) {
  if (window != owner_)
    return;

  // Changes to the container's bounds require all windows to be laid out.
  for (auto child : window->children())
    LayoutWindow(child);
}

void WindowLayout::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (window == owner_)
    return;

  // Changes to the following properties require the window to be laid out.
  if (name == mus::mojom::WindowManager::kPreferredSize_Property ||
      name == mus::mojom::WindowManager::kShowState_Property) {
    LayoutWindow(window);
  }
}

void WindowLayout::LayoutWindow(mus::Window* window) {
  mus::mojom::ShowState show_state = GetWindowShowState(window);
  mojo::Rect user_set_bounds = GetWindowUserSetBounds(window);
  mojo::Size preferred_size = GetWindowPreferredSize(window);

  // Maximized/fullscreen/presentation windows should be sized to the bounds
  // of the container.
  // If a window has bounds set by the user, those should be respected as long
  // as they meet certain constraints (e.g. visible).
  // TODO(beng): transient windows:
  // Top level non-transient windows with no bounds but a preferred size should
  // opened centered within the work area.
  // Transient windows should be opened centered within their parent.

  switch (show_state) {
    case mus::mojom::SHOW_STATE_MAXIMIZED:
    case mus::mojom::SHOW_STATE_IMMERSIVE:
    case mus::mojom::SHOW_STATE_PRESENTATION:
      FitToContainer(window);
      break;
    case mus::mojom::SHOW_STATE_RESTORED: {
      if (!IsRectEmpty(user_set_bounds)) {
        // If the bounds are unchanged, this will do nothing.
        window->SetBounds(user_set_bounds);
      } else if (!IsSizeEmpty(preferred_size)) {
        CenterWindow(window, preferred_size);
      }
    }
    case mus::mojom::SHOW_STATE_MINIMIZED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void WindowLayout::FitToContainer(mus::Window* window) {
  mojo::Rect container_bounds = owner_->bounds();
  container_bounds.x = 0;
  container_bounds.y = 0;
  window->SetBounds(container_bounds);
}

void WindowLayout::CenterWindow(mus::Window* window,
                                const mojo::Size& preferred_size) {
  mojo::Rect bounds;
  bounds.x = (owner_->bounds().width - preferred_size.width) / 2;
  bounds.y = (owner_->bounds().height - preferred_size.height) / 2;
  bounds.width = preferred_size.width;
  bounds.height = preferred_size.height;
  window->SetBounds(bounds);
}
