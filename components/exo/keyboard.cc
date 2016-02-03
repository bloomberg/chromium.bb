// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "ash/shell.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Keyboard, public:

Keyboard::Keyboard(KeyboardDelegate* delegate)
    : delegate_(delegate), focus_(nullptr), modifier_flags_(0) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

Keyboard::~Keyboard() {
  delegate_->OnKeyboardDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
      ->RemoveObserver(this);
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Keyboard::OnKeyEvent(ui::KeyEvent* event) {
  // These modifiers reflect what Wayland is aware of.  For example,
  // EF_SCROLL_LOCK_ON is missing because Wayland doesn't support scroll lock.
  const int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                            ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                            ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN |
                            ui::EF_NUM_LOCK_ON | ui::EF_CAPS_LOCK_ON;
  int modifier_flags = event->flags() & kModifierMask;
  if (modifier_flags != modifier_flags_) {
    modifier_flags_ = modifier_flags;
    if (focus_)
      delegate_->OnKeyboardModifiers(modifier_flags_);
  }

  switch (event->type()) {
    case ui::ET_KEY_PRESSED: {
      auto it =
          std::find(pressed_keys_.begin(), pressed_keys_.end(), event->code());
      if (it == pressed_keys_.end()) {
        if (focus_)
          delegate_->OnKeyboardKey(event->time_stamp(), event->code(), true);

        pressed_keys_.push_back(event->code());
      }
    } break;
    case ui::ET_KEY_RELEASED: {
      auto it =
          std::find(pressed_keys_.begin(), pressed_keys_.end(), event->code());
      if (it != pressed_keys_.end()) {
        if (focus_)
          delegate_->OnKeyboardKey(event->time_stamp(), event->code(), false);

        pressed_keys_.erase(it);
      }
    } break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::FocusChangeObserver overrides:

void Keyboard::OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) {
  Surface* gained_focus_surface =
      gained_focus ? GetEffectiveFocus(gained_focus) : nullptr;
  if (gained_focus_surface != focus_) {
    if (focus_) {
      delegate_->OnKeyboardLeave(focus_);
      focus_->RemoveSurfaceObserver(this);
      focus_ = nullptr;
    }
    if (gained_focus_surface) {
      delegate_->OnKeyboardModifiers(modifier_flags_);
      delegate_->OnKeyboardEnter(gained_focus_surface, pressed_keys_);
      focus_ = gained_focus_surface;
      focus_->AddSurfaceObserver(this);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Keyboard::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  focus_ = nullptr;
  surface->RemoveSurfaceObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard, private:

Surface* Keyboard::GetEffectiveFocus(aura::Window* window) const {
  Surface* main_surface =
      ShellSurface::GetMainSurface(window->GetToplevelWindow());
  Surface* window_surface = Surface::AsSurface(window);

  // Use window surface as effective focus and fallback to main surface when
  // needed.
  Surface* focus = window_surface ? window_surface : main_surface;
  if (!focus)
    return nullptr;

  return delegate_->CanAcceptKeyboardEventsForSurface(focus) ? focus : nullptr;
}

}  // namespace exo
