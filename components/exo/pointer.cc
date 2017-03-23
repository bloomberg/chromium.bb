// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform_util.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/cursor_factory_ozone.h"
#endif

#if defined(USE_X11)
#include "ui/base/cursor/cursor_loader_x11.h"
#endif

namespace exo {
namespace {

const float kLargeCursorScale = 2.8f;

// Synthesized events typically lack floating point precision so to avoid
// generating mouse event jitter we consider the location of these events
// to be the same as |location| if floored values match.
bool SameLocation(const ui::LocatedEvent* event, const gfx::PointF& location) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return event->location() == gfx::ToFlooredPoint(location);

  return event->location_f() == location;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate)
    : delegate_(delegate),
      cursor_(ui::kCursorNull),
      cursor_capture_source_id_(base::UnguessableToken::Create()),
      cursor_capture_weak_ptr_factory_(this) {
  auto* helper = WMHelper::GetInstance();
  helper->AddPreTargetHandler(this);
  helper->AddCursorObserver(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (surface_)
    surface_->RemoveSurfaceObserver(this);
  if (focus_) {
    focus_->RemoveSurfaceObserver(this);
    focus_->UnregisterCursorProvider(this);
  }
  auto* helper = WMHelper::GetInstance();
  helper->RemoveCursorObserver(this);
  helper->RemovePreTargetHandler(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_)
    return;

  // This is used to avoid unnecessary cursor changes.
  bool cursor_changed = false;

  // If surface is different than the current pointer surface then remove the
  // current surface and add the new surface.
  if (surface != surface_) {
    if (surface && surface->HasSurfaceDelegate()) {
      DLOG(ERROR) << "Surface has already been assigned a role";
      return;
    }
    if (surface_) {
      surface_->window()->SetTransform(gfx::Transform());
      WMHelper::GetInstance()
          ->GetContainer(ash::kShellWindowId_MouseCursorContainer)
          ->RemoveChild(surface_->window());
      surface_->SetSurfaceDelegate(nullptr);
      surface_->RemoveSurfaceObserver(this);
    }
    surface_ = surface;
    if (surface_) {
      surface_->SetSurfaceDelegate(this);
      surface_->AddSurfaceObserver(this);
      // Note: Surface window needs to be added to the tree so we can take a
      // snapshot. Where in the tree is not important but we might as well use
      // the cursor container.
      WMHelper::GetInstance()
          ->GetContainer(ash::kShellWindowId_MouseCursorContainer)
          ->AddChild(surface_->window());
    }
    cursor_changed = true;
  }

  // Update hotspot.
  if (hotspot != hotspot_) {
    hotspot_ = hotspot;
    cursor_changed = true;
  }

  // Early out if cursor did not change.
  if (!cursor_changed)
    return;

  // If |surface_| is set then ascynchrounsly capture a snapshot of cursor,
  // otherwise cancel pending capture and immediately set the cursor to "none".
  if (surface_) {
    CaptureCursor();
  } else {
    cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
    cursor_ = ui::kCursorNone;
    UpdateCursor();
  }
}

gfx::NativeCursor Pointer::GetCursor() {
  return cursor_;
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
      // Require SetCursor() to be called and cursor to be re-defined in
      // response to each OnPointerEnter() call.
      focus_->UnregisterCursorProvider(this);
      focus_ = nullptr;
      cursor_ = ui::kCursorNull;
      cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
    }
    // Second generate an enter event if focus moved to a new target.
    if (target) {
      delegate_->OnPointerEnter(target, event->location_f(),
                                event->button_flags());
      location_ = event->location_f();
      focus_ = target;
      focus_->AddSurfaceObserver(this);
      focus_->RegisterCursorProvider(this);
    }
    delegate_->OnPointerFrame();
  }

  if (focus_ && event->IsMouseEvent() && event->type() != ui::ET_MOUSE_EXITED) {
    // Generate motion event if location changed. We need to check location
    // here as mouse movement can generate both "moved" and "entered" events
    // but OnPointerMotion should only be called if location changed since
    // OnPointerEnter was called.
    if (!SameLocation(event, location_)) {
      location_ = event->location_f();
      delegate_->OnPointerMotion(event->time_stamp(), location_);
      delegate_->OnPointerFrame();
    }
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_RELEASED:
      if (focus_) {
        delegate_->OnPointerButton(event->time_stamp(),
                                   event->changed_button_flags(),
                                   event->type() == ui::ET_MOUSE_PRESSED);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL:
      if (focus_) {
        ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
        delegate_->OnPointerScroll(
            event->time_stamp(),
            gfx::Vector2dF(scroll_event->x_offset(), scroll_event->y_offset()),
            false);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_MOUSEWHEEL:
      if (focus_) {
        delegate_->OnPointerScroll(
            event->time_stamp(),
            static_cast<ui::MouseWheelEvent*>(event)->offset(), true);
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL_FLING_START:
      if (focus_) {
        delegate_->OnPointerScrollStop(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
      if (focus_) {
        delegate_->OnPointerScrollCancel(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }

  if (focus_)
    UpdateCursorScale();
}

void Pointer::OnScrollEvent(ui::ScrollEvent* event) {
  OnMouseEvent(event);
}

void Pointer::OnCursorSetChanged(ui::CursorSetType cursor_set) {
  if (focus_)
    UpdateCursorScale();
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void Pointer::OnSurfaceCommit() {
  surface_->CheckIfSurfaceHierarchyNeedsCommitToNewSurfaces();
  surface_->CommitSurfaceHierarchy();

  // Capture new cursor to reflect result of commit.
  if (focus_)
    CaptureCursor();
}

bool Pointer::IsSurfaceSynchronized() const {
  // A pointer surface is always desynchronized.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Pointer::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == surface_ || surface == focus_);
  if (surface == surface_)
    surface_ = nullptr;
  if (surface == focus_)
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

void Pointer::UpdateCursorScale() {
  DCHECK(focus_);

  display::Screen* screen = display::Screen::GetScreen();
  WMHelper* helper = WMHelper::GetInstance();

  // Update cursor scale if the effective UI scale has changed.
  display::Display display = screen->GetDisplayNearestWindow(focus_->window());
  float scale = helper->GetDisplayInfo(display.id()).GetEffectiveUIScale();

  if (display::Display::HasInternalDisplay()) {
    float primary_device_scale_factor =
        screen->GetPrimaryDisplay().device_scale_factor();
    // The size of the cursor surface is the quotient of its physical size and
    // the DSF of the primary display. The physical size is proportional to the
    // DSF of the internal display. For external displays (and the internal
    // display when secondary to a display with a different DSF), scale the
    // cursor so its physical size matches with the single display case.
    if (!display.IsInternal() ||
        display.device_scale_factor() != primary_device_scale_factor) {
      scale *= primary_device_scale_factor /
               helper->GetDisplayInfo(display::Display::InternalDisplayId())
                   .device_scale_factor();
    }
  }

  if (helper->GetCursorSet() == ui::CURSOR_SET_LARGE)
    scale *= kLargeCursorScale;

  if (scale != cursor_scale_) {
    cursor_scale_ = scale;
    if (surface_)
      CaptureCursor();
  }
}

void Pointer::CaptureCursor() {
  DCHECK(surface_);

  // Set UI scale before submitting capture request.
  surface_->window()->layer()->SetTransform(
      gfx::GetScaleTransform(gfx::Point(), cursor_scale_));

  float primary_device_scale_factor =
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();

  std::unique_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(
          base::Bind(&Pointer::OnCursorCaptured,
                     cursor_capture_weak_ptr_factory_.GetWeakPtr(),
                     gfx::ScaleToFlooredPoint(
                         hotspot_,
                         // |hotspot_| is in surface coordinate space so apply
                         // both device scale and UI scale.
                         cursor_scale_ * primary_device_scale_factor)));
  request->set_source(cursor_capture_source_id_);
  surface_->window()->layer()->RequestCopyOfOutput(std::move(request));
}

void Pointer::OnCursorCaptured(const gfx::Point& hotspot,
                               std::unique_ptr<cc::CopyOutputResult> result) {
  if (!focus_)
    return;

  cursor_ = ui::kCursorNone;
  if (!result->IsEmpty()) {
    DCHECK(result->HasBitmap());
    std::unique_ptr<SkBitmap> bitmap = result->TakeBitmap();

    ui::PlatformCursor platform_cursor;
#if defined(USE_OZONE)
    // TODO(reveman): Add interface for creating cursors from GpuMemoryBuffers
    // and use that here instead of the current bitmap API. crbug.com/686600
    platform_cursor = ui::CursorFactoryOzone::GetInstance()->CreateImageCursor(
        *bitmap.get(), hotspot);
#elif defined(USE_X11)
    XcursorImage* image = ui::SkBitmapToXcursorImage(bitmap.get(), hotspot);
    platform_cursor = ui::CreateReffedCustomXCursor(image);
#endif
    cursor_ = ui::kCursorCustom;
    cursor_.SetPlatformCursor(platform_cursor);
#if defined(USE_OZONE)
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor);
#elif defined(USE_X11)
    ui::UnrefCustomXCursor(platform_cursor);
#endif
  }

  UpdateCursor();
}

void Pointer::UpdateCursor() {
  DCHECK(focus_);

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(focus_->window()->GetRootWindow());
  if (cursor_client)
    cursor_client->SetCursor(cursor_);
}

}  // namespace exo
