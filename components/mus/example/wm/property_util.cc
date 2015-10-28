// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/property_util.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"

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

ash::mojom::Container GetRequestedContainer(mus::Window* window) {
  if (window->HasSharedProperty(ash::mojom::kWindowContainer_Property)) {
    return static_cast<ash::mojom::Container>(
        window->GetSharedProperty<int32_t>(
            ash::mojom::kWindowContainer_Property));
  }
  return ash::mojom::CONTAINER_USER_WINDOWS;
}
