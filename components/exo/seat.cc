// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include "ash/shell.h"
#include "components/exo/keyboard.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/focus_client.h"

namespace exo {
namespace {

Surface* GetEffectiveFocus(aura::Window* window) {
  if (!window)
    return nullptr;
  Surface* const surface = Surface::AsSurface(window);
  if (surface)
    return surface;
  // Fallback to main surface.
  aura::Window* const top_level_window = window->GetToplevelWindow();
  if (!top_level_window)
    return nullptr;
  return ShellSurface::GetMainSurface(top_level_window);
}

}  // namespace

Seat::Seat() {
  aura::client::GetFocusClient(ash::Shell::Get()->GetPrimaryRootWindow())
      ->AddObserver(this);
  WMHelper::GetInstance()->AddPreTargetHandler(this);
}

Seat::~Seat() {
  aura::client::GetFocusClient(ash::Shell::Get()->GetPrimaryRootWindow())
      ->RemoveObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
}

void Seat::AddObserver(SeatObserver* observer) {
  observers_.AddObserver(observer);
}

void Seat::RemoveObserver(SeatObserver* observer) {
  observers_.RemoveObserver(observer);
}

Surface* Seat::GetFocusedSurface() {
  return GetEffectiveFocus(WMHelper::GetInstance()->GetFocusedWindow());
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Seat::OnWindowFocused(aura::Window* gained_focus,
                           aura::Window* lost_focus) {
  Surface* const surface = GetEffectiveFocus(gained_focus);
  for (auto& observer : observers_) {
    observer.OnSurfaceFocusing(surface);
  }
  for (auto& observer : observers_) {
    observer.OnSurfaceFocused(surface);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Seat::OnKeyEvent(ui::KeyEvent* event) {
  switch (event->type()) {
    case ui::ET_KEY_PRESSED:
      pressed_keys_.insert(event->code());
      break;
    case ui::ET_KEY_RELEASED:
      pressed_keys_.erase(event->code());
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace exo
