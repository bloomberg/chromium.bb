// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_H_
#define COMPONENTS_EXO_SURFACE_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace exo {
class Buffer;
class SurfaceDelegate;

// This class represents a rectangular area that is displayed on the screen.
// It has a location, size and pixel contents.
class Surface : public views::View, public ui::CompositorObserver {
 public:
  Surface();
  ~Surface() override;

  // Set a buffer as the content of this surface. A buffer can only be attached
  // to one surface at a time.
  void Attach(Buffer* buffer);

  // Describe the regions where the pending buffer is different from the
  // current surface contents, and where the surface therefore needs to be
  // repainted.
  void Damage(const gfx::Rect& rect);

  // Request notification when the next frame is displayed. Useful for
  // throttling redrawing operations, and driving animations.
  using FrameCallback = base::Callback<void(base::TimeTicks frame_time)>;
  void RequestFrameCallback(const FrameCallback& callback);

  // This sets the region of the surface that contains opaque content.
  void SetOpaqueRegion(const SkRegion& region);

  // Surface state (damage regions, attached buffers, etc.) is double-buffered.
  // A Commit() call atomically applies all pending state, replacing the
  // current state.
  void Commit();

  // Set the surface delegate.
  void SetSurfaceDelegate(SurfaceDelegate* delegate);

  // Returns a trace value representing the state of the surface.
  scoped_refptr<base::trace_event::TracedValue> AsTracedValue() const;

  bool HasPendingDamageForTesting() const { return !pending_damage_.IsEmpty(); }

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;

  // Overridden from ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override {}
  void OnCompositingAborted(ui::Compositor* compositor) override {}
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

 private:
  // The buffer that will become the content of surface when Commit() is called.
  base::WeakPtr<Buffer> pending_buffer_;

  // The damage region to schedule paint for when Commit() is called.
  // TODO(reveman): Use SkRegion here after adding a version of
  // ui::Layer::SchedulePaint that takes a SkRegion.
  gfx::Rect pending_damage_;

  // These lists contains the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. Later they are moved to
  // |active_frame_callbacks_| when the effect of the Commit() is reflected in
  // the compositor's active layer tree. The callbacks fire once we're notified
  // that the compositor started drawing that active layer tree.
  std::list<FrameCallback> pending_frame_callbacks_;
  std::list<FrameCallback> frame_callbacks_;
  std::list<FrameCallback> active_frame_callbacks_;

  // The opaque region to take effect when Commit() is called.
  SkRegion pending_opaque_region_;

  // The compsitor being observer or null if not observing a compositor.
  ui::Compositor* compositor_;

  // This can be set to have some functions delegated. E.g. ShellSurface class
  // can set this to handle Commit() and apply any double buffered state it
  // maintains.
  SurfaceDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_H_
