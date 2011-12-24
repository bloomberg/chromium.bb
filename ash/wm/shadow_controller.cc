// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shadow_controller.h"

#include <utility>

#include "ash/ash_switches.h"
#include "ash/wm/shadow.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/window_properties.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"

using std::make_pair;

namespace ash {
namespace internal {

namespace {

ShadowType GetShadowTypeFromWindowType(aura::Window* window) {
  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
      return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAuraTranslucentFrames) ?
              SHADOW_TYPE_NONE : SHADOW_TYPE_RECTANGULAR;
    case aura::client::WINDOW_TYPE_MENU:
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return SHADOW_TYPE_RECTANGULAR;
    default:
      break;
  }
  return SHADOW_TYPE_NONE;
}

}  // namespace

ShadowController::ShadowController() {
  aura::RootWindow::GetInstance()->AddRootWindowObserver(this);
}

ShadowController::~ShadowController() {
  for (WindowShadowMap::const_iterator it = window_shadows_.begin();
       it != window_shadows_.end(); ++it) {
    it->first->RemoveObserver(this);
  }
  aura::RootWindow::GetInstance()->RemoveRootWindowObserver(this);
}

void ShadowController::OnWindowInitialized(aura::Window* window) {
  window->AddObserver(this);
  SetShadowType(window, GetShadowTypeFromWindowType(window));
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowPropertyChanged(aura::Window* window,
                                               const char* name,
                                               void* old) {
  if (name == kShadowTypeKey)
    HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& bounds) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->SetContentBounds(gfx::Rect(bounds.size()));
}

void ShadowController::OnWindowDestroyed(aura::Window* window) {
  window_shadows_.erase(window);
}

bool ShadowController::ShouldShowShadowForWindow(aura::Window* window) const {
  const ShadowType type = GetShadowType(window);
  switch (type) {
    case SHADOW_TYPE_NONE:
      return false;
    case SHADOW_TYPE_RECTANGULAR:
      return true;
    default:
      NOTREACHED() << "Unknown shadow type " << type;
      return false;
  }
}

Shadow* ShadowController::GetShadowForWindow(aura::Window* window) {
  WindowShadowMap::const_iterator it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : NULL;
}

void ShadowController::HandlePossibleShadowVisibilityChange(
    aura::Window* window) {
  const bool should_show = ShouldShowShadowForWindow(window);
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->layer()->SetVisible(should_show);
  else if (should_show && !shadow)
    CreateShadowForWindow(window);
}

void ShadowController::CreateShadowForWindow(aura::Window* window) {
  linked_ptr<Shadow> shadow(new Shadow());
  window_shadows_.insert(make_pair(window, shadow));

  shadow->Init();
  shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));
  window->layer()->Add(shadow->layer());
}

}  // namespace internal
}  // namespace ash
