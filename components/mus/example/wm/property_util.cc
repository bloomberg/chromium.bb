// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/property_util.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

mus::mojom::ShowState GetWindowShowState(mus::Window* window) {
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

gfx::Rect GetWindowUserSetBounds(mus::Window* window) {
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

gfx::Size GetWindowPreferredSize(mus::Window* window) {
  if (window->HasSharedProperty(
      mus::mojom::WindowManager::kPreferredSize_Property)) {
    return window->GetSharedProperty<gfx::Size>(
        mus::mojom::WindowManager::kPreferredSize_Property);
  }
  return gfx::Size();
}

ash::mojom::Container GetRequestedContainer(mus::Window* window) {
  if (window->HasSharedProperty(ash::mojom::kWindowContainer_Property)) {
    return static_cast<ash::mojom::Container>(
        window->GetSharedProperty<int32_t>(
            ash::mojom::kWindowContainer_Property));
  }
  return ash::mojom::CONTAINER_USER_WINDOWS;
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
