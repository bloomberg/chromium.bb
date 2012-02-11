// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shadow_types.h"

#include "ash/wm/window_properties.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

void SetShadowType(aura::Window* window, ShadowType shadow_type) {
  window->SetProperty(kShadowTypeKey, shadow_type);
}

ShadowType GetShadowType(aura::Window* window) {
  return window->GetProperty(kShadowTypeKey);
}

}  // namespace internal
}  // namespace ash
