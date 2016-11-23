// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_PROPERTY_UTIL_H_
#define ASH_MUS_PROPERTY_UTIL_H_

#include "ash/public/interfaces/ash_window_type.mojom.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/wm/public/window_types.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {
class Window;
}

namespace ash {
namespace mus {

class Shadow;

// Utility functions to read values from properties & convert them to the
// appropriate types.

void SetWindowShowState(ui::Window* window, ui::mojom::ShowState show_state);
ui::mojom::ShowState GetWindowShowState(const ui::Window* window);

void SetWindowUserSetBounds(ui::Window* window, const gfx::Rect& bounds);
gfx::Rect GetWindowUserSetBounds(const ui::Window* window);

void SetWindowPreferredSize(ui::Window* window, const gfx::Size& size);
gfx::Size GetWindowPreferredSize(const ui::Window* window);

// If |window| has the |kInitialContainerId_Property| set as a property, then
// the value of |kInitialContainerId_Property| is set in |container_id| and true
// is returned. Otherwise false is returned.
bool GetRequestedContainer(const ui::Window* window, int* container_id);

// Returns a bitfield of kResizeBehavior* values from
// window_manager_constants.mojom.
void SetResizeBehavior(ui::Window::SharedProperties* properties,
                       int32_t resize_behavior);
int32_t GetResizeBehavior(const ui::Window* window);

void SetRestoreBounds(ui::Window* window, const gfx::Rect& bounds);
gfx::Rect GetRestoreBounds(const ui::Window* window);

void SetShadow(ui::Window* window, Shadow* shadow);
Shadow* GetShadow(const ui::Window* window);

ui::mojom::WindowType GetWindowType(const ui::Window* window);
ui::mojom::WindowType GetWindowType(const ui::Window::SharedProperties& window);

ui::wm::WindowType GetWmWindowType(const ui::Window* window);

mojom::AshWindowType GetAshWindowType(const ui::Window* window);

void SetWindowTitle(ui::Window* window, base::string16 title);
base::string16 GetWindowTitle(const ui::Window* window);

void SetAppID(ui::Window* window, const base::string16& app_id);
base::string16 GetAppID(const ui::Window* window);

bool GetWindowIgnoredByShelf(ui::Window* window);

void SetWindowIsJanky(ui::Window* window, bool janky);
bool IsWindowJanky(ui::Window* window);
bool IsWindowJankyProperty(const void* key);

void SetAlwaysOnTop(ui::Window* window, bool value);
bool IsAlwaysOnTop(ui::Window* window);

bool ShouldRemoveStandardFrame(ui::Window* window);

// See description of |WindowManager::kRendererParentTitleArea_Property|.
bool ShouldRenderParentTitleArea(ui::Window* window);

// Returns the kInitialDisplayId_Property if present, otherwise
// kInvalidDisplayID.
int64_t GetInitialDisplayId(const ui::Window::SharedProperties& properties);

// Manipulates the kExcludeFromMru_Property property.
void SetExcludeFromMru(ui::Window* window, bool value);

// Returns true if the property is set and true, otherwise false.
bool GetExcludeFromMru(const ui::Window* window);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_PROPERTY_UTIL_H_
