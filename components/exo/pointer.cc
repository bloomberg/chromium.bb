// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/pointer.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "components/exo/buffer.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/surface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

scoped_ptr<Buffer> CreateDefaultCursor(gfx::Point* hotspot) {
  ui::Cursor cursor(ui::kCursorPointer);
  cursor.set_device_scale_factor(1.0f);

  SkBitmap bitmap;
  if (!ui::GetCursorBitmap(cursor, &bitmap, hotspot)) {
    LOG(ERROR) << "Failed to load default cursor bitmap";
    return nullptr;
  }

  DCHECK_EQ(bitmap.colorType(), kBGRA_8888_SkColorType);
  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->AllocateGpuMemoryBuffer(gfx::SkISizeToSize(bitmap.dimensions()),
                                    gfx::BufferFormat::BGRA_8888,
                                    gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  bool rv = gpu_memory_buffer->Map();
  DCHECK(rv);
  int stride = gpu_memory_buffer->stride(0 /* plane */);
  DCHECK_GT(stride, 0);
  bitmap.copyPixelsTo(gpu_memory_buffer->memory(0 /* plane */),
                      stride * bitmap.height(), stride);
  gpu_memory_buffer->Unmap();

  return make_scoped_ptr(new Buffer(std::move(gpu_memory_buffer)));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Pointer, public:

Pointer::Pointer(PointerDelegate* delegate)
    : delegate_(delegate), surface_(nullptr), focus_(nullptr) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

Pointer::~Pointer() {
  delegate_->OnPointerDestroying(this);
  if (surface_)
    surface_->RemoveSurfaceObserver(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);
  if (widget_)
    widget_->CloseNow();
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void Pointer::SetCursor(Surface* surface, const gfx::Point& hotspot) {
  // Early out if the pointer doesn't have a surface in focus.
  if (!focus_)
    return;

  // If surface is different than the current pointer surface then remove the
  // current surface and add the new surface.
  if (surface != surface_) {
    if (surface && surface->HasSurfaceDelegate()) {
      DLOG(ERROR) << "Surface has already been assigned a role";
      return;
    }
    if (surface_) {
      widget_->GetNativeWindow()->RemoveChild(surface_);
      surface_->Hide();
      surface_->SetSurfaceDelegate(nullptr);
      surface_->RemoveSurfaceObserver(this);
    }
    surface_ = surface;
    if (surface_) {
      surface_->SetSurfaceDelegate(this);
      surface_->AddSurfaceObserver(this);
      widget_->GetNativeWindow()->AddChild(surface_);
    }
  }

  // Update hotspot and show cursor surface if not already visible.
  hotspot_ = hotspot;
  if (surface_) {
    surface_->SetBounds(gfx::Rect(gfx::Point() - hotspot_.OffsetFromOrigin(),
                                  surface_->layer()->size()));
    if (!surface_->IsVisible())
      surface_->Show();
  }
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
      delegate_->OnPointerEnter(target, event->location(),
                                event->button_flags());
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
    case ui::ET_SCROLL:
      if (focus_) {
        ui::ScrollEvent* scroll_event = static_cast<ui::ScrollEvent*>(event);
        delegate_->OnPointerWheel(
            event->time_stamp(),
            gfx::Vector2d(scroll_event->x_offset(), scroll_event->y_offset()));
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

  // Update cursor widget to reflect current focus and pointer location.
  if (focus_) {
    if (!widget_) {
      CreatePointerWidget();

      // Create default pointer surface. This will be used until a different
      // pointer surface is set using SetCursor().
      default_cursor_ = CreateDefaultCursor(&hotspot_);
      default_surface_.reset(new Surface);
      surface_ = default_surface_.get();
      surface_->SetSurfaceDelegate(this);
      surface_->AddSurfaceObserver(this);
      widget_->GetNativeWindow()->AddChild(surface_);
      surface_->Attach(default_cursor_.get());
      surface_->Commit();
      surface_->Show();
    }
    widget_->SetBounds(gfx::Rect(
        focus_->GetBoundsInScreen().origin() + location_.OffsetFromOrigin(),
        gfx::Size(1, 1)));
    if (!widget_->IsVisible())
      widget_->Show();
  } else {
    if (widget_ && widget_->IsVisible())
      widget_->Hide();
  }
}

void Pointer::OnScrollEvent(ui::ScrollEvent* event) {
  OnMouseEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void Pointer::OnSurfaceCommit() {
  surface_->CommitSurfaceHierarchy();
  surface_->SetBounds(gfx::Rect(gfx::Point() - hotspot_.OffsetFromOrigin(),
                                surface_->layer()->size()));
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

void Pointer::CreatePointerWidget() {
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_TOOLTIP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_MouseCursorContainer);
  widget_.reset(new views::Widget);
  widget_->Init(params);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
  widget_->GetNativeWindow()->SetName("ExoPointer");
}

Surface* Pointer::GetEffectiveTargetForEvent(ui::Event* event) const {
  Surface* target =
      Surface::AsSurface(static_cast<aura::Window*>(event->target()));
  if (!target)
    return nullptr;

  return delegate_->CanAcceptPointerEventsForSurface(target) ? target : nullptr;
}

}  // namespace exo
