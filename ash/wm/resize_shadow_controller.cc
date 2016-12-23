// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow_controller.h"

#include <utility>

#include "ash/wm/resize_shadow.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"

namespace ash {

ResizeShadowController::ResizeShadowController() {}

ResizeShadowController::~ResizeShadowController() {
  for (const auto& shadow : window_shadows_)
    shadow.first->RemoveObserver(this);
}

void ResizeShadowController::ShowShadow(aura::Window* window, int hit_test) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (!shadow)
    shadow = CreateShadow(window);
  shadow->ShowForHitTest(hit_test);
}

void ResizeShadowController::HideShadow(aura::Window* window) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->Hide();
}

ResizeShadow* ResizeShadowController::GetShadowForWindowForTest(
    aura::Window* window) {
  return GetShadowForWindow(window);
}

void ResizeShadowController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  ResizeShadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->Layout(new_bounds);
}

void ResizeShadowController::OnWindowDestroyed(aura::Window* window) {
  window_shadows_.erase(window);
}

ResizeShadow* ResizeShadowController::CreateShadow(aura::Window* window) {
  auto shadow = base::MakeUnique<ResizeShadow>();
  // Attach the layers to this window.
  shadow->Init(window);
  // Ensure initial bounds are correct.
  shadow->Layout(window->bounds());
  // Watch for bounds changes.
  window->AddObserver(this);

  ResizeShadow* raw_shadow = shadow.get();
  window_shadows_.insert(std::make_pair(window, std::move(shadow)));
  return raw_shadow;
}

ResizeShadow* ResizeShadowController::GetShadowForWindow(aura::Window* window) {
  auto it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : nullptr;
}

}  // namespace ash
