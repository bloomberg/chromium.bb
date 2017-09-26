// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/pointer_stylus_delegate.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor_util.h"
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

// TODO(oshima): Some accessibility features, including large cursors, disable
// hardware cursors. Ash does not support compositing for custom cursors, so it
// replaces them with the default cursor. As a result, this scale has no effect
// for now. See crbug.com/708378.
const float kLargeCursorScale = 2.8f;

const double kLocatedEventEpsilonSquared = 1.0 / (2000.0 * 2000.0);

// Synthesized events typically lack floating point precision so to avoid
// generating mouse event jitter we consider the location of these events
// to be the same as |location| if floored values match.
bool SameLocation(const ui::LocatedEvent* event, const gfx::PointF& location) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return event->location() == gfx::ToFlooredPoint(location);

  // In general, it is good practice to compare floats using an epsilon.
  // In particular, the mouse location_f() could differ between the
  // MOUSE_PRESSED and MOUSE_RELEASED events. At MOUSE_RELEASED, it will have a
  // targeter() already cached, while at MOUSE_PRESSED, it will have to
  // calculate it passing through all the hierarchy of windows, and that could
  // generate rounding error. std::numeric_limits<float>::epsilon() is not big
  // enough to catch this rounding error.
  gfx::Vector2dF offset = event->location_f() - location;
  return offset.LengthSquared() < (2 * kLocatedEventEpsilonSquared);
}

display::ManagedDisplayInfo GetCaptureDisplayInfo() {
  display::ManagedDisplayInfo capture_info;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    const auto& info = WMHelper::GetInstance()->GetDisplayInfo(display.id());
    if (info.device_scale_factor() >= capture_info.device_scale_factor())
      capture_info = info;
  }
  return capture_info;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate)
    : SurfaceTreeHost("ExoPointer", nullptr),
      delegate_(delegate),
      cursor_(ui::CursorType::kNull),
      capture_scale_(GetCaptureDisplayInfo().device_scale_factor()),
      capture_ratio_(GetCaptureDisplayInfo().GetDensityRatio()),
      cursor_capture_source_id_(base::UnguessableToken::Create()),
      cursor_capture_weak_ptr_factory_(this) {
  auto* helper = WMHelper::GetInstance();
  helper->AddPreTargetHandler(this);
  helper->AddCursorObserver(this);
  helper->AddDisplayConfigurationObserver(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (focus_surface_) {
    focus_surface_->RemoveSurfaceObserver(this);
    focus_surface_->UnregisterCursorProvider(this);
  }
  auto* helper = WMHelper::GetInstance();
  helper->RemoveDisplayConfigurationObserver(this);
  helper->RemoveCursorObserver(this);
  helper->RemovePreTargetHandler(this);
  if (root_surface())
    root_surface()->RemoveSurfaceObserver(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_surface_)
    return;

  // This is used to avoid unnecessary cursor changes.
  bool cursor_changed = false;

  // If surface is different than the current pointer surface then remove the
  // current surface and add the new surface.
  if (surface != root_surface()) {
    if (surface && surface->HasSurfaceDelegate()) {
      DLOG(ERROR) << "Surface has already been assigned a role";
      return;
    }
    UpdatePointerSurface(surface);
    cursor_changed = true;
  }

  if (hotspot != cursor_hotspot_) {
    hotspot_ = hotspot;
    cursor_changed = true;
  }

  // Early out if cursor did not change.
  if (!cursor_changed) {
    // Cursor scale or rotation may have changed.
    UpdateCursor();
    return;
  }

  // If |SurfaceTreeHost::root_surface_| is set then asynchronously capture a
  // snapshot of cursor, otherwise cancel pending capture and immediately set
  // the cursor to "none".
  if (root_surface()) {
    CaptureCursor(hotspot);
  } else {
    cursor_bitmap_.reset();
    cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
    UpdateCursor();
  }
}

gfx::NativeCursor Pointer::GetCursor() {
  return cursor_;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void Pointer::OnSurfaceCommit() {
  SurfaceTreeHost::OnSurfaceCommit();

  // Capture new cursor to reflect result of commit.
  if (focus_surface_)
    CaptureCursor(hotspot_);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Pointer::OnSurfaceDestroying(Surface* surface) {
  if (surface == focus_surface_) {
    focus_surface_ = nullptr;
    surface->RemoveSurfaceObserver(this);
    return;
  }
  if (surface == root_surface()) {
    UpdatePointerSurface(nullptr);
    return;
  }
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Pointer::OnMouseEvent(ui::MouseEvent* event) {
  Surface* target = GetEffectiveTargetForEvent(event);

  // If target is different than the current pointer focus then we need to
  // generate enter and leave events.
  if (target != focus_surface_) {
    // First generate a leave event if we currently have a target in focus.
    if (focus_surface_) {
      delegate_->OnPointerLeave(focus_surface_);
      focus_surface_->RemoveSurfaceObserver(this);
      // Require SetCursor() to be called and cursor to be re-defined in
      // response to each OnPointerEnter() call.
      focus_surface_->UnregisterCursorProvider(this);
      focus_surface_ = nullptr;
      cursor_ = ui::CursorType::kNull;
      cursor_capture_weak_ptr_factory_.InvalidateWeakPtrs();
    }
    // Second generate an enter event if focus moved to a new target.
    if (target) {
      delegate_->OnPointerEnter(target, event->location_f(),
                                event->button_flags());
      location_ = event->location_f();
      focus_surface_ = target;
      focus_surface_->AddSurfaceObserver(this);
      focus_surface_->RegisterCursorProvider(this);
    }
    delegate_->OnPointerFrame();
  }

  if (!focus_surface_)
    return;

  if (event->IsMouseEvent() && event->type() != ui::ET_MOUSE_EXITED) {
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
    case ui::ET_MOUSE_RELEASED: {
      delegate_->OnPointerButton(event->time_stamp(),
                                 event->changed_button_flags(),
                                 event->type() == ui::ET_MOUSE_PRESSED);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL: {
      ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
      delegate_->OnPointerScroll(
          event->time_stamp(),
          gfx::Vector2dF(scroll_event->x_offset(), scroll_event->y_offset()),
          false);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_MOUSEWHEEL: {
      delegate_->OnPointerScroll(
          event->time_stamp(),
          static_cast<ui::MouseWheelEvent*>(event)->offset(), true);
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL_FLING_START: {
      // Fling start in chrome signals the lifting of fingers after scrolling.
      // In wayland terms this signals the end of a scroll sequence.
      delegate_->OnPointerScrollStop(event->time_stamp());
      delegate_->OnPointerFrame();
      break;
    }
    case ui::ET_SCROLL_FLING_CANCEL: {
      // Fling cancel is generated very generously at every touch of the
      // touchpad. Since it's not directly supported by the delegate, we do not
      // want limit this event to only right after a fling start has been
      // generated to prevent erronous behavior.
      if (last_event_type_ == ui::ET_SCROLL_FLING_START) {
        // We emulate fling cancel by starting a new scroll sequence that
        // scrolls by 0 pixels, effectively stopping any kinetic scroll motion.
        delegate_->OnPointerScroll(event->time_stamp(), gfx::Vector2dF(),
                                   false);
        delegate_->OnPointerFrame();
        delegate_->OnPointerScrollStop(event->time_stamp());
        delegate_->OnPointerFrame();
      }
      break;
    }
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

  last_event_type_ = event->type();
}

void Pointer::OnScrollEvent(ui::ScrollEvent* event) {
  OnMouseEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper::CursorObserver overrides:

void Pointer::OnCursorSizeChanged(ui::CursorSize cursor_size) {
  if (focus_surface_)
    UpdateCursor();
}

void Pointer::OnCursorDisplayChanged(const display::Display& display) {
  if (focus_surface_)
    UpdateCursor();
}

////////////////////////////////////////////////////////////////////////////////
// WMHelper::DisplayConfigurationObserver overrides:

void Pointer::OnDisplayConfigurationChanged() {
  UpdatePointerSurface(root_surface());
  auto info = GetCaptureDisplayInfo();
  capture_scale_ = info.device_scale_factor();
  capture_ratio_ = info.GetDensityRatio();
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

void Pointer::UpdatePointerSurface(Surface* surface) {
  if (root_surface()) {
    host_window()->SetTransform(gfx::Transform());
    if (host_window()->parent())
      host_window()->parent()->RemoveChild(host_window());
    root_surface()->RemoveSurfaceObserver(this);
    SetRootSurface(nullptr);
  }

  if (surface) {
    surface->AddSurfaceObserver(this);
    // Note: Surface window needs to be added to the tree so we can take a
    // snapshot. Where in the tree is not important but we might as well use
    // the cursor container.
    WMHelper::GetInstance()
        ->GetPrimaryDisplayContainer(ash::kShellWindowId_MouseCursorContainer)
        ->AddChild(host_window());
    SetRootSurface(surface);
  }
}

void Pointer::CaptureCursor(const gfx::Point& hotspot) {
  DCHECK(root_surface());
  DCHECK(focus_surface_);

  // Surface size is in DIPs, while layer size is in pseudo-DIP units that
  // depend on the DSF of the display mode. Scale the layer to capture the
  // surface at a constant pixel size, regardless of the primary display's
  // UI scale and display mode DSF.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* helper = WMHelper::GetInstance();
  float scale = helper->GetDisplayInfo(display.id()).GetEffectiveUIScale() *
                capture_scale_ / display.device_scale_factor();
  host_window()->SetTransform(gfx::GetScaleTransform(gfx::Point(), scale));

  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(&Pointer::OnCursorCaptured,
                         cursor_capture_weak_ptr_factory_.GetWeakPtr(),
                         hotspot));

  request->set_source(cursor_capture_source_id_);
  host_window()->layer()->RequestCopyOfOutput(std::move(request));
}

void Pointer::OnCursorCaptured(const gfx::Point& hotspot,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  if (!focus_surface_)
    return;

  if (result->IsEmpty()) {
    cursor_bitmap_.reset();
  } else {
    cursor_bitmap_ = result->AsSkBitmap();
    DCHECK(cursor_bitmap_.readyToDraw());
    cursor_hotspot_ = hotspot;
  }

  UpdateCursor();
}

void Pointer::UpdateCursor() {
  DCHECK(focus_surface_);

  if (cursor_bitmap_.drawsNothing()) {
    cursor_ = ui::CursorType::kNone;
  } else {
    SkBitmap bitmap = cursor_bitmap_;
    gfx::Point hotspot =
        gfx::ScaleToFlooredPoint(cursor_hotspot_, capture_ratio_);

    auto* helper = WMHelper::GetInstance();
    const display::Display& display = helper->GetCursorDisplay();
    float scale =
        helper->GetDisplayInfo(display.id()).GetDensityRatio() / capture_ratio_;

    if (helper->GetCursorSize() == ui::CursorSize::kLarge)
      scale *= kLargeCursorScale;

    ui::ScaleAndRotateCursorBitmapAndHotpoint(scale, display.rotation(),
                                              &bitmap, &hotspot);

    ui::PlatformCursor platform_cursor;
#if defined(USE_OZONE)
    // TODO(reveman): Add interface for creating cursors from GpuMemoryBuffers
    // and use that here instead of the current bitmap API. crbug.com/686600
    platform_cursor = ui::CursorFactoryOzone::GetInstance()->CreateImageCursor(
        bitmap, hotspot, 0);
#elif defined(USE_X11)
    XcursorImage* image = ui::SkBitmapToXcursorImage(&bitmap, hotspot);
    platform_cursor = ui::CreateReffedCustomXCursor(image);
#endif
    cursor_ = ui::CursorType::kCustom;
    cursor_.SetPlatformCursor(platform_cursor);
#if defined(USE_OZONE)
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor);
#elif defined(USE_X11)
    ui::UnrefCustomXCursor(platform_cursor);
#endif
  }

  aura::Window* root_window = focus_surface_->window()->GetRootWindow();
  if (!root_window)
    return;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(cursor_);
}

}  // namespace exo
