// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/shell.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate)
    : delegate_(delegate), focus_(nullptr) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  Surface* target = GetEffectiveTargetForEvent(event);

  // If target is different than the current pointer focus then we need to
  // generate enter and leave events.
  if (target != focus_) {
    // First generate a leave event if we currently have a target in focus.
    if (focus_) {
      delegate_->OnPointerLeave(focus_);
      focus_->RemoveSurfaceObserver(this);
      focus_ = nullptr;
    }
    // Second generate an enter event if focus moved to a new target.
    if (target) {
      int button_flags =
          event->flags() &
          (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
           ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
           ui::EF_FORWARD_MOUSE_BUTTON);
      delegate_->OnPointerEnter(target, event->location(), button_flags);
      location_ = event->location();
      focus_ = target;
      focus_->AddSurfaceObserver(this);
    }
  }
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      if (focus_) {
        delegate_->OnPointerButton(event->time_stamp(),
                                   event->changed_button_flags(),
                                   event->type() == ui::ET_MOUSE_PRESSED);
      }
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      // Generate motion event if location changed. We need to check location
      // here as mouse movement can generate both "moved" and "entered" events
      // but OnPointerMotion should only be called if location changed since
      // OnPointerEnter was called.
      if (focus_ && event->location() != location_) {
        delegate_->OnPointerMotion(event->time_stamp(), event->location());
        location_ = event->location();
      }
      break;
    case ui::ET_MOUSEWHEEL:
      if (focus_) {
        delegate_->OnPointerWheel(
            event->time_stamp(),
            static_cast<ui::MouseWheelEvent*>(event)->offset());
      }
      break;
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Pointer::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  focus_ = nullptr;
  surface->RemoveSurfaceObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// Pointer, private:

Surface* Pointer::GetEffectiveTargetForEvent(ui::Event* event) const {
  Surface* target =
      Surface::AsSurface(static_cast<aura::Window*>(event->target()));
  if (!target)
    return nullptr;

  return delegate_->CanAcceptPointerEventsForSurface(target) ? target : nullptr;
}

}  // namespace exo
