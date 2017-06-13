// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_H_
#define COMPONENTS_EXO_POINTER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wm_helper.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CopyOutputResult;
}

namespace ui {
class Event;
class MouseEvent;
}

namespace exo {
class PointerDelegate;
class Surface;

// This class implements a client pointer that represents one or more input
// devices, such as mice, which control the pointer location and pointer focus.
class Pointer : public ui::EventHandler,
                public WMHelper::CursorObserver,
                public WMHelper::DisplayConfigurationObserver,
                public SurfaceDelegate,
                public SurfaceObserver {
 public:
  explicit Pointer(PointerDelegate* delegate);
  ~Pointer() override;

  // Set the pointer surface, i.e., the surface that contains the pointer image
  // (cursor). The |hotspot| argument defines the position of the pointer
  // surface relative to the pointer location. Its top-left corner is always at
  // (x, y) - (hotspot.x, hotspot.y), where (x, y) are the coordinates of the
  // pointer location, in surface local coordinates.
  void SetCursor(Surface* surface, const gfx::Point& hotspot);

  // Returns the current cursor for the pointer.
  gfx::NativeCursor GetCursor();

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;

  // Overridden from WMHelper::CursorObserver:
  void OnCursorSetChanged(ui::CursorSetType cursor_set) override;
  void OnCursorDisplayChanged(const display::Display& display) override;

  // Overridden from WMHelper::DisplayConfigurationObserver:
  void OnDisplayConfigurationChanged() override;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::Event* event) const;

  // Updates the |surface_| from which the cursor is captured.
  void UpdatePointerSurface(Surface* surface);

  // Asynchronously update the cursor by capturing a snapshot of |surface_|.
  void CaptureCursor(const gfx::Point& hotspot);

  // Called when cursor snapshot has been captured.
  void OnCursorCaptured(const gfx::Point& hotspot,
                        std::unique_ptr<cc::CopyOutputResult> result);

  // Update |cursor_| to |cursor_bitmap_| transformed for the current display.
  void UpdateCursor();

  // The delegate instance that all events are dispatched to.
  PointerDelegate* const delegate_;

  // The current pointer surface.
  Surface* surface_ = nullptr;

  // The current focus surface for the pointer.
  Surface* focus_ = nullptr;

  // The location of the pointer in the current focus surface.
  gfx::PointF location_;

  // The position of the pointer surface relative to the pointer location.
  gfx::Point hotspot_;

  // Latest cursor snapshot.
  SkBitmap cursor_bitmap_;

  // The current cursor.
  ui::Cursor cursor_;

  // Source used for cursor capture copy output requests.
  const base::UnguessableToken cursor_capture_source_id_;

  // Last received event type.
  ui::EventType last_event_type_ = ui::ET_UNKNOWN;

  // Weak pointer factory used for cursor capture callbacks.
  base::WeakPtrFactory<Pointer> cursor_capture_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Pointer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_H_
