// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_PROPERTY_UTIL_H_
#define ASH_MUS_PROPERTY_UTIL_H_

#include "ash/public/interfaces/ash_window_type.mojom.h"
#include "ash/public/interfaces/container.mojom.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/interfaces/window_manager_constants.mojom.h"
#include "ui/wm/public/window_types.h"

namespace gfx {
class Rect;
class Size;
}

namespace mus {
class Window;
}

namespace ash {
namespace mus {

class Shadow;

// Utility functions to read values from properties & convert them to the
// appropriate types.

void SetWindowShowState(::mus::Window* window,
                        ::mus::mojom::ShowState show_state);
::mus::mojom::ShowState GetWindowShowState(const ::mus::Window* window);

void SetWindowUserSetBounds(::mus::Window* window, const gfx::Rect& bounds);
gfx::Rect GetWindowUserSetBounds(const ::mus::Window* window);

void SetWindowPreferredSize(::mus::Window* window, const gfx::Size& size);
gfx::Size GetWindowPreferredSize(const ::mus::Window* window);

// If |window| has the |kWindowContainer_Property| set as a property, then
// the value of |kWindowContainer_Property| is set in |container| and true is
// returned. Otherwise false is returned.
bool GetRequestedContainer(const ::mus::Window* window,
                           mojom::Container* container);

// Returns a bitfield of kResizeBehavior* values from
// window_manager_constants.mojom.
int32_t GetResizeBehavior(const ::mus::Window* window);

void SetRestoreBounds(::mus::Window* window, const gfx::Rect& bounds);
gfx::Rect GetRestoreBounds(const ::mus::Window* window);

void SetShadow(::mus::Window* window, Shadow* shadow);
Shadow* GetShadow(const ::mus::Window* window);

::mus::mojom::WindowType GetWindowType(const ::mus::Window* window);
::mus::mojom::WindowType GetWindowType(
    const ::mus::Window::SharedProperties& window);

ui::wm::WindowType GetWmWindowType(const ::mus::Window* window);

mojom::AshWindowType GetAshWindowType(const ::mus::Window* window);

base::string16 GetWindowTitle(const ::mus::Window* window);

mojo::Array<uint8_t> GetWindowAppIcon(const ::mus::Window* window);

void SetAppID(::mus::Window* window, const base::string16& app_id);
base::string16 GetAppID(const ::mus::Window* window);

bool GetWindowIgnoredByShelf(::mus::Window* window);

void SetWindowIsJanky(::mus::Window* window, bool janky);
bool IsWindowJanky(::mus::Window* window);
bool IsWindowJankyProperty(const void* key);

void SetAlwaysOnTop(::mus::Window* window, bool value);
bool IsAlwaysOnTop(::mus::Window* window);

bool ShouldRemoveStandardFrame(::mus::Window* window);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_PROPERTY_UTIL_H_
