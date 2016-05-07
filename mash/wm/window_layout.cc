// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/window_layout.h"

#include <stdint.h>

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/property_util.h"

namespace mash {
namespace wm {

WindowLayout::WindowLayout(mus::Window* owner) : LayoutManager(owner) {
  AddLayoutProperty(mus::mojom::WindowManager::kPreferredSize_Property);
  AddLayoutProperty(mus::mojom::WindowManager::kShowState_Property);
}
WindowLayout::~WindowLayout() {}

void WindowLayout::LayoutWindow(mus::Window* window) {
  mus::mojom::ShowState show_state = GetWindowShowState(window);
  gfx::Rect user_set_bounds = GetWindowUserSetBounds(window);
  gfx::Size preferred_size = GetWindowPreferredSize(window);

  // Maximized/fullscreen windows should be sized to the bounds of the
  // container.
  // If a window has bounds set by the user, those should be respected as long
  // as they meet certain constraints (e.g. visible).
  // TODO(beng): transient windows:
  // Top level non-transient windows with no bounds but a preferred size should
  // opened centered within the work area.
  // Transient windows should be opened centered within their parent.

  switch (show_state) {
    case mus::mojom::ShowState::FULLSCREEN:
    case mus::mojom::ShowState::MAXIMIZED:
      FitToContainer(window);
      break;

    case mus::mojom::ShowState::DEFAULT:
    case mus::mojom::ShowState::DOCKED:
    case mus::mojom::ShowState::INACTIVE:
    case mus::mojom::ShowState::NORMAL: {
      if (!user_set_bounds.IsEmpty()) {
        // If the bounds are unchanged, this will do nothing.
        window->SetBounds(user_set_bounds);
      } else if (!preferred_size.IsEmpty()) {
        CenterWindow(window, preferred_size);
      }
    }
    case mus::mojom::ShowState::MINIMIZED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void WindowLayout::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  // TODO(sky): this feels like the wrong place for this logic. Find a better
  // place.
  if (name == mus::mojom::WindowManager::kShowState_Property &&
      GetWindowShowState(window) == mus::mojom::ShowState::MAXIMIZED) {
    SetRestoreBounds(window, window->bounds());
  }
  LayoutManager::OnWindowSharedPropertyChanged(window, name, old_data,
                                               new_data);
}

void WindowLayout::FitToContainer(mus::Window* window) {
  window->SetBounds(gfx::Rect(owner()->bounds().size()));
}

void WindowLayout::CenterWindow(mus::Window* window,
                                const gfx::Size& preferred_size) {
  const gfx::Rect bounds(
      (owner()->bounds().width() - preferred_size.width()) / 2,
      (owner()->bounds().height() - preferred_size.height()) / 2,
      preferred_size.width(), preferred_size.height());
  window->SetBounds(bounds);
}

}  // namespace wm
}  // namespace mash
