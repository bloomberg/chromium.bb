// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/property_util.h"

#include <stdint.h>

#include "ash/mus/shadow.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(ash::mus::Shadow*,
                                     kLocalShadowProperty,
                                     nullptr);
MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(bool, kWindowIsJankyProperty, false);

}  // namespace

namespace ash {
namespace mus {

void SetWindowShowState(ui::Window* window, ui::mojom::ShowState show_state) {
  window->SetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kShowState_Property,
      static_cast<uint32_t>(show_state));
}

ui::mojom::ShowState GetWindowShowState(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kShowState_Property)) {
    return static_cast<ui::mojom::ShowState>(window->GetSharedProperty<int32_t>(
        ui::mojom::WindowManager::kShowState_Property));
  }
  return ui::mojom::ShowState::DEFAULT;
}

void SetWindowUserSetBounds(ui::Window* window, const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    window->ClearSharedProperty(
        ui::mojom::WindowManager::kUserSetBounds_Property);
  } else {
    window->SetSharedProperty<gfx::Rect>(
        ui::mojom::WindowManager::kUserSetBounds_Property, bounds);
  }
}

gfx::Rect GetWindowUserSetBounds(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kUserSetBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        ui::mojom::WindowManager::kUserSetBounds_Property);
  }
  return gfx::Rect();
}

void SetWindowPreferredSize(ui::Window* window, const gfx::Size& size) {
  window->SetSharedProperty<gfx::Size>(
      ui::mojom::WindowManager::kPreferredSize_Property, size);
}

gfx::Size GetWindowPreferredSize(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kPreferredSize_Property)) {
    return window->GetSharedProperty<gfx::Size>(
        ui::mojom::WindowManager::kPreferredSize_Property);
  }
  return gfx::Size();
}

bool GetRequestedContainer(const ui::Window* window, int* container_id) {
  if (!window->HasSharedProperty(
          ui::mojom::WindowManager::kInitialContainerId_Property))
    return false;

  *container_id = window->GetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kInitialContainerId_Property);
  return true;
}

void SetResizeBehavior(ui::Window::SharedProperties* properties,
                       int32_t resize_behavior) {
  (*properties)[ui::mojom::WindowManager::kResizeBehavior_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(resize_behavior);
}

int32_t GetResizeBehavior(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kResizeBehavior_Property)) {
    return window->GetSharedProperty<int32_t>(
        ui::mojom::WindowManager::kResizeBehavior_Property);
  }
  return ui::mojom::kResizeBehaviorNone;
}

void SetRestoreBounds(ui::Window* window, const gfx::Rect& bounds) {
  window->SetSharedProperty<gfx::Rect>(
      ui::mojom::WindowManager::kRestoreBounds_Property, bounds);
}

gfx::Rect GetRestoreBounds(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kRestoreBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        ui::mojom::WindowManager::kRestoreBounds_Property);
  }
  return gfx::Rect();
}

void SetShadow(ui::Window* window, Shadow* shadow) {
  window->SetLocalProperty(kLocalShadowProperty, shadow);
}

Shadow* GetShadow(const ui::Window* window) {
  return window->GetLocalProperty(kLocalShadowProperty);
}

ui::mojom::WindowType GetWindowType(const ui::Window* window) {
  if (window->HasSharedProperty(
          ui::mojom::WindowManager::kWindowType_Property)) {
    return static_cast<ui::mojom::WindowType>(
        window->GetSharedProperty<int32_t>(
            ui::mojom::WindowManager::kWindowType_Property));
  }
  return ui::mojom::WindowType::POPUP;
}

ui::mojom::WindowType GetWindowType(
    const ui::Window::SharedProperties& properties) {
  const auto iter =
      properties.find(ui::mojom::WindowManager::kWindowType_Property);
  if (iter != properties.end()) {
    return static_cast<ui::mojom::WindowType>(
        mojo::ConvertTo<int32_t>(iter->second));
  }
  return ui::mojom::WindowType::POPUP;
}

ui::wm::WindowType GetWmWindowType(const ui::Window* window) {
  switch (GetWindowType(window)) {
    case ui::mojom::WindowType::WINDOW:
      return ui::wm::WINDOW_TYPE_NORMAL;

    case ui::mojom::WindowType::PANEL:
      return ui::wm::WINDOW_TYPE_PANEL;

    case ui::mojom::WindowType::CONTROL:
      return ui::wm::WINDOW_TYPE_CONTROL;

    case ui::mojom::WindowType::WINDOW_FRAMELESS:
    case ui::mojom::WindowType::POPUP:
    case ui::mojom::WindowType::BUBBLE:
    case ui::mojom::WindowType::DRAG:
      return ui::wm::WINDOW_TYPE_POPUP;

    case ui::mojom::WindowType::MENU:
      return ui::wm::WINDOW_TYPE_MENU;

    case ui::mojom::WindowType::TOOLTIP:
      return ui::wm::WINDOW_TYPE_TOOLTIP;

    case ui::mojom::WindowType::UNKNOWN:
      return ui::wm::WINDOW_TYPE_UNKNOWN;
  }

  return ui::wm::WINDOW_TYPE_UNKNOWN;
}

mojom::AshWindowType GetAshWindowType(const ui::Window* window) {
  if (!window->HasSharedProperty(mojom::kAshWindowType_Property))
    return mojom::AshWindowType::COUNT;

  return static_cast<mojom::AshWindowType>(
      window->GetSharedProperty<int32_t>(mojom::kAshWindowType_Property));
}

void SetWindowTitle(ui::Window* window, base::string16 title) {
  window->SetSharedProperty<base::string16>(
      ui::mojom::WindowManager::kWindowTitle_Property, title);
}

base::string16 GetWindowTitle(const ui::Window* window) {
  if (!window->HasSharedProperty(
          ui::mojom::WindowManager::kWindowTitle_Property)) {
    return base::string16();
  }

  return window->GetSharedProperty<base::string16>(
      ui::mojom::WindowManager::kWindowTitle_Property);
}

void SetAppID(ui::Window* window, const base::string16& app_id) {
  window->SetSharedProperty<base::string16>(
      ui::mojom::WindowManager::kAppID_Property, app_id);
}

base::string16 GetAppID(const ui::Window* window) {
  if (!window->HasSharedProperty(ui::mojom::WindowManager::kAppID_Property))
    return base::string16();

  return window->GetSharedProperty<base::string16>(
      ui::mojom::WindowManager::kAppID_Property);
}

bool GetWindowIgnoredByShelf(ui::Window* window) {
  return window->HasSharedProperty(
             ui::mojom::WindowManager::kWindowIgnoredByShelf_Property) &&
         window->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kWindowIgnoredByShelf_Property);
}

void SetWindowIsJanky(ui::Window* window, bool janky) {
  window->SetLocalProperty(kWindowIsJankyProperty, janky);
}

bool IsWindowJanky(ui::Window* window) {
  return window->GetLocalProperty(kWindowIsJankyProperty);
}

bool IsWindowJankyProperty(const void* key) {
  return key == kWindowIsJankyProperty;
}

void SetAlwaysOnTop(ui::Window* window, bool value) {
  window->SetSharedProperty<bool>(
      ui::mojom::WindowManager::kAlwaysOnTop_Property, value);
}

bool IsAlwaysOnTop(ui::Window* window) {
  return window->HasSharedProperty(
             ui::mojom::WindowManager::kAlwaysOnTop_Property) &&
         window->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kAlwaysOnTop_Property);
}

bool ShouldRemoveStandardFrame(ui::Window* window) {
  return window->HasSharedProperty(
             ui::mojom::WindowManager::kRemoveStandardFrame_Property) &&
         window->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kRemoveStandardFrame_Property);
}

bool ShouldRenderParentTitleArea(ui::Window* window) {
  return window->HasSharedProperty(
             ui::mojom::WindowManager::kRendererParentTitleArea_Property) &&
         window->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kRendererParentTitleArea_Property);
}

int64_t GetInitialDisplayId(const ui::Window::SharedProperties& properties) {
  auto iter =
      properties.find(ui::mojom::WindowManager::kInitialDisplayId_Property);
  return iter == properties.end() ? display::Display::kInvalidDisplayID
                                  : mojo::ConvertTo<int64_t>(iter->second);
}

void SetExcludeFromMru(ui::Window* window, bool value) {
  window->SetSharedProperty<bool>(
      ui::mojom::WindowManager::kExcludeFromMru_Property, value);
}

bool GetExcludeFromMru(const ui::Window* window) {
  return window->HasSharedProperty(
             ui::mojom::WindowManager::kExcludeFromMru_Property) &&
         window->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kExcludeFromMru_Property);
}

}  // namespace mus
}  // namespace ash
