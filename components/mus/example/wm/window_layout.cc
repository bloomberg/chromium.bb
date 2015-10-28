// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_layout.h"

#include "components/mus/example/wm/property_util.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "ui/mojo/geometry/geometry_util.h"

WindowLayout::WindowLayout(mus::Window* owner) : LayoutManager(owner) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
  AddLayoutProperty(mus::mojom::WindowManager::kShowState_Property);
}
WindowLayout::~WindowLayout() {}

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
      if (user_set_bounds != mojo::Rect()) {
        // If the bounds are unchanged, this will do nothing.
        window->SetBounds(user_set_bounds);
      } else if (preferred_size != mojo::Size()) {
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
  mojo::Rect container_bounds = owner()->bounds();
  container_bounds.x = 0;
  container_bounds.y = 0;
  window->SetBounds(container_bounds);
}

void WindowLayout::CenterWindow(mus::Window* window,
                                const mojo::Size& preferred_size) {
  mojo::Rect bounds;
  bounds.x = (owner()->bounds().width - preferred_size.width) / 2;
  bounds.y = (owner()->bounds().height - preferred_size.height) / 2;
  bounds.width = preferred_size.width;
  bounds.height = preferred_size.height;
  window->SetBounds(bounds);
}
