// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/property_util.h"

#include <stdint.h>

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_property.h"
#include "mash/wm/shadow.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace mash {
namespace wm {
namespace {

MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(Shadow*, kLocalShadowProperty, nullptr);

}  // namespace

void SetWindowShowState(mus::Window* window, mus::mojom::ShowState show_state) {
  window->SetSharedProperty<int32_t>(
      mus::mojom::WindowManager::kShowState_Property,
      static_cast<uint32_t>(show_state));
}

mus::mojom::ShowState GetWindowShowState(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kShowState_Property)) {
    return static_cast<mus::mojom::ShowState>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kShowState_Property));
  }
  return mus::mojom::ShowState::RESTORED;
}

void SetWindowUserSetBounds(mus::Window* window, const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    window->ClearSharedProperty(
        mus::mojom::WindowManager::kUserSetBounds_Property);
  } else {
    window->SetSharedProperty<gfx::Rect>(
        mus::mojom::WindowManager::kUserSetBounds_Property, bounds);
  }
}

gfx::Rect GetWindowUserSetBounds(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kUserSetBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        mus::mojom::WindowManager::kUserSetBounds_Property);
  }
  return gfx::Rect();
}

void SetWindowPreferredSize(mus::Window* window, const gfx::Size& size) {
  window->SetSharedProperty<gfx::Size>(
      mus::mojom::WindowManager::kPreferredSize_Property, size);
}

gfx::Size GetWindowPreferredSize(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kPreferredSize_Property)) {
    return window->GetSharedProperty<gfx::Size>(
        mus::mojom::WindowManager::kPreferredSize_Property);
  }
  return gfx::Size();
}

mojom::Container GetRequestedContainer(const mus::Window* window) {
  if (window->HasSharedProperty(mojom::kWindowContainer_Property)) {
    return static_cast<mojom::Container>(
        window->GetSharedProperty<int32_t>(mojom::kWindowContainer_Property));
  }
  return mojom::Container::USER_WINDOWS;
}

int32_t GetResizeBehavior(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kResizeBehavior_Property)) {
    return window->GetSharedProperty<int32_t>(
        mus::mojom::WindowManager::kResizeBehavior_Property);
  }
  return mus::mojom::kResizeBehaviorNone;
}

void SetRestoreBounds(mus::Window* window, const gfx::Rect& bounds) {
  window->SetSharedProperty<gfx::Rect>(
      mus::mojom::WindowManager::kRestoreBounds_Property, bounds);
}

gfx::Rect GetRestoreBounds(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kRestoreBounds_Property)) {
    return window->GetSharedProperty<gfx::Rect>(
        mus::mojom::WindowManager::kRestoreBounds_Property);
  }
  return gfx::Rect();
}

void SetShadow(mus::Window* window, Shadow* shadow) {
  window->SetLocalProperty(kLocalShadowProperty, shadow);
}

Shadow* GetShadow(const mus::Window* window) {
  return window->GetLocalProperty(kLocalShadowProperty);
}

mus::mojom::WindowType GetWindowType(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kWindowType_Property)) {
    return static_cast<mus::mojom::WindowType>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kWindowType_Property));
  }
  return mus::mojom::WindowType::POPUP;
}

mus::mojom::WindowType GetWindowType(
    const mus::Window::SharedProperties& properties) {
  const auto iter =
      properties.find(mus::mojom::WindowManager::kWindowType_Property);
  if (iter != properties.end()) {
    return static_cast<mus::mojom::WindowType>(
        mojo::ConvertTo<int32_t>(iter->second));
  }
  return mus::mojom::WindowType::POPUP;
}

base::string16 GetWindowTitle(const mus::Window* window) {
  if (!window->HasSharedProperty(
          mus::mojom::WindowManager::kWindowTitle_Property)) {
    return base::string16();
  }

  return window->GetSharedProperty<base::string16>(
      mus::mojom::WindowManager::kWindowTitle_Property);
}

mojo::Array<uint8_t> GetWindowAppIcon(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kWindowAppIcon_Property)) {
    return mojo::Array<uint8_t>::From(
        window->GetSharedProperty<std::vector<uint8_t>>(
            mus::mojom::WindowManager::kWindowAppIcon_Property));
  }
  return mojo::Array<uint8_t>();
}

void SetAppID(mus::Window* window, const base::string16& app_id) {
  window->SetSharedProperty<base::string16>(
      mus::mojom::WindowManager::kAppID_Property, app_id);
}

base::string16 GetAppID(const mus::Window* window) {
  if (!window->HasSharedProperty(mus::mojom::WindowManager::kAppID_Property))
    return base::string16();

  return window->GetSharedProperty<base::string16>(
      mus::mojom::WindowManager::kAppID_Property);
}

}  // namespace wm
}  // namespace mash
