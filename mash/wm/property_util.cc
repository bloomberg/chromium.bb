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

mus::mojom::ShowState GetWindowShowState(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kShowState_Property)) {
    return static_cast<mus::mojom::ShowState>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kShowState_Property));
  }
  return mus::mojom::SHOW_STATE_RESTORED;
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
  return mojom::CONTAINER_USER_WINDOWS;
}

mus::mojom::ResizeBehavior GetResizeBehavior(const mus::Window* window) {
  if (window->HasSharedProperty(
          mus::mojom::WindowManager::kResizeBehavior_Property)) {
    return static_cast<mus::mojom::ResizeBehavior>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kResizeBehavior_Property));
  }
  return mus::mojom::RESIZE_BEHAVIOR_NONE;
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

Shadow* GetShadow(mus::Window* window) {
  return window->GetLocalProperty(kLocalShadowProperty);
}

mus::mojom::WindowType GetWindowType(mus::Window* window) {
  return GetWindowType(window->shared_properties());
}

mus::mojom::WindowType GetWindowType(
    const mus::Window::SharedProperties& properties) {
  const auto iter =
      properties.find(mus::mojom::WindowManager::kWindowType_Property);
  if (iter != properties.end()) {
    return static_cast<mus::mojom::WindowType>(
        mojo::TypeConverter<int32_t, const std::vector<uint8_t>>::Convert(
            iter->second));
  }
  return mus::mojom::WINDOW_TYPE_POPUP;
}

}  // namespace wm
}  // namespace mash
