// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_PROPERTY_UTIL_H_
#define MASH_WM_PROPERTY_UTIL_H_

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "mash/wm/public/interfaces/container.mojom.h"

namespace gfx {
class Rect;
class Size;
}

namespace mus {
class Window;
}

namespace mash {
namespace wm {

class Shadow;

// Utility functions to read values from properties & convert them to the
// appropriate types.

mus::mojom::ShowState GetWindowShowState(const mus::Window* window);

void SetWindowUserSetBounds(mus::Window* window, const gfx::Rect& bounds);
gfx::Rect GetWindowUserSetBounds(const mus::Window* window);

void SetWindowPreferredSize(mus::Window* window, const gfx::Size& size);
gfx::Size GetWindowPreferredSize(const mus::Window* window);

mojom::Container GetRequestedContainer(const mus::Window* window);

// Returns a bitfield of kResizeBehavior* values from
// window_manager_constants.mojom.
int32_t GetResizeBehavior(const mus::Window* window);

void SetRestoreBounds(mus::Window* window, const gfx::Rect& bounds);
gfx::Rect GetRestoreBounds(const mus::Window* window);

void SetShadow(mus::Window* window, Shadow* shadow);
Shadow* GetShadow(mus::Window* window);

mus::mojom::WindowType GetWindowType(mus::Window* window);
mus::mojom::WindowType GetWindowType(
    const mus::Window::SharedProperties& window);

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_PROPERTY_UTIL_H_
