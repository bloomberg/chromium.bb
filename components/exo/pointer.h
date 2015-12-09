// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_H_
#define COMPONENTS_EXO_POINTER_H_

#include "base/macros.h"
#include "components/exo/surface_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class Event;
class MouseEvent;
class MouseWheelEvent;
}

namespace exo {
class PointerDelegate;
class Surface;

// This class implements a client pointer that represents one or more input
// devices, such as mice, which control the pointer location and pointer focus.
class Pointer : public ui::EventHandler, public SurfaceObserver {
 public:
  explicit Pointer(PointerDelegate* delegate);
  ~Pointer() override;

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::Event* event) const;

  // The delegate instance that all events are dispatched to.
  PointerDelegate* delegate_;

  // The current focus surface for the pointer.
  Surface* focus_;

  // The location of the pointer in the current focus surface.
  gfx::Point location_;

  DISALLOW_COPY_AND_ASSIGN(Pointer);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_H_
