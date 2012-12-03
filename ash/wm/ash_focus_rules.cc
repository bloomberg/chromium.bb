// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_focus_rules.h"

#include "ui/aura/window.h"

namespace ash {
namespace wm {

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, public:

AshFocusRules::AshFocusRules() {
}

AshFocusRules::~AshFocusRules() {
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, views::corewm::FocusRules:

bool AshFocusRules::CanActivateWindow(aura::Window* window) {
  return window && !!window->parent();
}

bool AshFocusRules::CanFocusWindow(aura::Window* window) {
  aura::Window* activatable = GetActivatableWindow(window);
  return activatable->Contains(window) && window->CanFocus();
}

aura::Window* AshFocusRules::GetActivatableWindow(aura::Window* window) {
  return window;
}

aura::Window* AshFocusRules::GetFocusableWindow(aura::Window* window) {
  return window;
}

aura::Window* AshFocusRules::GetNextActivatableWindow(aura::Window* ignore) {
  return NULL;
}

aura::Window* AshFocusRules::GetNextFocusableWindow(aura::Window* ignore) {
  return GetFocusableWindow(ignore->parent());
}

}  // namespace wm
}  // namespace ash
