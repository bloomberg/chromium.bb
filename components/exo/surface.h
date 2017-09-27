// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_H_
#define COMPONENTS_EXO_SURFACE_H_

#include <list>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface_delegate.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace gfx {
class Path;
}

namespace viz {
class CompositorFrame;
}

namespace exo {
class Buffer;
class LayerTreeFrameSinkHolder;
class Pointer;
class SurfaceObserver;
class Surface;

namespace subtle {
class PropertyHelper;
}

// Counter-clockwise rotations.
enum class Transform { NORMAL, ROTATE_90, ROTATE_180, ROTATE_270 };

// The pointer class is currently the only cursor provider class but this can
// change in the future when better hardware cursor support is added.
using CursorProvider = Pointer;

// This class represents a rectangular area that is displayed on the screen.
// It has a location, size and pixel contents.
class Surface final : public ui::PropertyHandler {
 public:
  using PropertyDeallocator = void (*)(int64_t value);

  Surface();
  ~Surface();

  // Type-checking downcast routine.
  static Surface* AsSurface(const aura::Window* window);

  aura::Window* window() { return window_.get(); }

  // Set a buffer as the content of this surface. A buffer can only be attached
  // to one surface at a time.
  void Attach(Buffer* buffer);

  // Describe the regions where the pending buffer is different from the
  // current surface contents, and where the surface therefore needs to be
  // repainted.
  void Damage(const gfx::Rect& rect);

  // Request notification when it's a good time to produce a new frame. Useful
  // for throttling redrawing operations, and driving animations.
  using FrameCallback = base::Callback<void(base::TimeTicks frame_time)>;
  void RequestFrameCallback(const FrameCallback& callback);

  // Request notification when the next frame is displayed. Useful for
  // throttling redrawing operations, and driving animations.
  using PresentationCallback =
      base::Callback<void(base::TimeTicks presentation_time,
                          base::TimeDelta refresh)>;
  void RequestPresentationCallback(const PresentationCallback& callback);

  // This sets the region of the surface that contains opaque content.
  void SetOpaqueRegion(const SkRegion& region);

  // This sets the region of the surface that can receive pointer and touch
  // events.
  void SetInputRegion(const SkRegion& region);

  // This sets the scaling factor used to interpret the contents of the buffer
  // attached to the surface. Note that if the scale is larger than 1, then you
  // have to attach a buffer that is larger (by a factor of scale in each
  // dimension) than the desired surface size.
  void SetBufferScale(float scale);

  // This sets the transformation used to interpret the contents of the buffer
  // attached to the surface.
  void SetBufferTransform(Transform transform);

  // Functions that control sub-surface state. All sub-surface state is
  // double-buffered and will be applied when Commit() is called.
  void AddSubSurface(Surface* sub_surface);
  void RemoveSubSurface(Surface* sub_surface);
  void SetSubSurfacePosition(Surface* sub_surface, const gfx::Point& position);
  void PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference);
  void PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling);

  // This sets the surface viewport for scaling.
  void SetViewport(const gfx::Size& viewport);

  // This sets the surface crop rectangle.
  void SetCrop(const gfx::RectF& crop);

  // This sets the only visible on secure output flag, preventing it from
  // appearing in screenshots or from being viewed on non-secure displays.
  void SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output);

  // This sets the blend mode that will be used when drawing the surface.
  void SetBlendMode(SkBlendMode blend_mode);

  // This sets the alpha value that will be applied to the whole surface.
  void SetAlpha(float alpha);

  // Request that surface should have the specified frame type.
  void SetFrame(SurfaceFrameType type);

  // Surface state (damage regions, attached buffers, etc.) is double-buffered.
  // A Commit() call atomically applies all pending state, replacing the
  // current state. Commit() is not guaranteed to be synchronous. See
  // CommitSurfaceHierarchy() below.
  void Commit();

  // This will synchronously commit all pending state of the surface and its
  // descendants by recursively calling CommitSurfaceHierarchy() for each
  // sub-surface with pending state.
  void CommitSurfaceHierarchy(
      const gfx::Point& origin,
      std::list<FrameCallback>* frame_callbacks,
      std::list<PresentationCallback>* presentation_callbacks);

  void AppendSurfaceHierarchyContentsToFrame(
      const gfx::Point& origin,
      float device_scale_factor,
      LayerTreeFrameSinkHolder* frame_sink_holder,
      viz::CompositorFrame* frame);

  // Returns true if surface is in synchronized mode.
  bool IsSynchronized() const;

  // Returns the bounds of the current input region of surface.
  gfx::Rect GetHitTestBounds() const;

  // Returns true if |rect| intersects this surface's bounds.
  bool HitTestRect(const gfx::Rect& rect) const;

  // Returns true if the current input region is different than the surface
  // bounds.
  bool HasHitTestMask() const;

  // Returns the current input region of surface in the form of a hit-test mask.
  void GetHitTestMask(gfx::Path* mask) const;

  // Returns the current input region of surface in the form of a set of
  // hit-test rects.
  std::unique_ptr<aura::WindowTargeter::HitTestRects> GetHitTestShapeRects()
      const;

  // Surface does not own cursor providers. It is the responsibility of the
  // caller to remove the cursor provider before it is destroyed.
  void RegisterCursorProvider(CursorProvider* provider);
  void UnregisterCursorProvider(CursorProvider* provider);

  // Returns the cursor for the surface. If no cursor provider is registered
  // then CursorType::kNull is returned.
  gfx::NativeCursor GetCursor();

  // Set the surface delegate.
  void SetSurfaceDelegate(SurfaceDelegate* delegate);

  // Returns true if surface has been assigned a surface delegate.
  bool HasSurfaceDelegate() const;

  // Surface does not own observers. It is the responsibility of the observer
  // to remove itself when it is done observing.
  void AddSurfaceObserver(SurfaceObserver* observer);
  void RemoveSurfaceObserver(SurfaceObserver* observer);
  bool HasSurfaceObserver(const SurfaceObserver* observer) const;

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Called when the begin frame source has changed.
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source);

  // Returns the active contents size.
  const gfx::Size& content_size() const { return content_size_; }

  // Returns true if the associated window is in 'stylus-only' mode.
  bool IsStylusOnly();

  // Enables 'stylus-only' mode for the associated window.
  void SetStylusOnly();

  // Notify surface that resources and subsurfaces' resources have been lost.
  void SurfaceHierarchyResourcesLost();

  // Returns true if the surface's bounds should be filled opaquely.
  bool FillsBoundsOpaquely() const;

  bool HasPendingDamageForTesting(const gfx::Rect& damage) const {
    return pending_damage_.contains(gfx::RectToSkIRect(damage));
  }

 private:
  struct State {
    State();
    ~State();

    bool operator==(const State& other);
    bool operator!=(const State& other) { return !(*this == other); }

    SkRegion opaque_region;
    SkRegion input_region;
    float buffer_scale = 1.0f;
    Transform buffer_transform = Transform::NORMAL;
    gfx::Size viewport;
    gfx::RectF crop;
    bool only_visible_on_secure_output = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;
    float alpha = 1.0f;
  };
  class BufferAttachment {
   public:
    BufferAttachment();
    ~BufferAttachment();

    BufferAttachment& operator=(BufferAttachment&& buffer);

    base::WeakPtr<Buffer>& buffer();
    const base::WeakPtr<Buffer>& buffer() const;
    void Reset(base::WeakPtr<Buffer> buffer);

   private:
    base::WeakPtr<Buffer> buffer_;

    DISALLOW_COPY_AND_ASSIGN(BufferAttachment);
  };

  friend class subtle::PropertyHelper;

  // Updates current_resource_ with a new resource id corresponding to the
  // contents of the attached buffer (or id 0, if no buffer is attached).
  // UpdateSurface must be called afterwards to ensure the release callback
  // will be called.
  void UpdateResource(LayerTreeFrameSinkHolder* frame_sink_holder);

  // Updates buffer_transform_ to match the current buffer parameters.
  void UpdateBufferTransform();

  // Puts the current surface into a draw quad, and appends the draw quads into
  // the |frame|.
  void AppendContentsToFrame(const gfx::Point& origin,
                             float device_scale_factor,
                             viz::CompositorFrame* frame);

  // Update surface content size base on current buffer size.
  void UpdateContentSize();

  // This returns true when the surface has some contents assigned to it.
  bool has_contents() const { return !!current_buffer_.buffer(); }

  // This window has the layer which contains the Surface contents.
  std::unique_ptr<aura::Window> window_;

  // This true, if sub_surfaces_ has changes (order, position, etc).
  bool sub_surfaces_changed_ = false;

  // This is the size of the last committed contents.
  gfx::Size content_size_;

  // This is true when Attach() has been called and new contents should take
  // effect next time Commit() is called.
  bool has_pending_contents_ = false;

  // The buffer that will become the content of surface when Commit() is called.
  BufferAttachment pending_buffer_;

  // The damage region to schedule paint for when Commit() is called.
  SkRegion pending_damage_;

  // The damage region which will be used by
  // AppendSurfaceHierarchyContentsToFrame() to generate frame.
  SkRegion damage_;

  // These lists contains the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. Later they are moved to
  // |active_frame_callbacks_| when the effect of the Commit() is scheduled to
  // be drawn. They fire at the first begin frame notification after this.
  std::list<FrameCallback> pending_frame_callbacks_;

  // These lists contains the callbacks to notify the client when surface
  // contents have been presented. These callbacks move to
  // |presentation_callbacks_| when Commit() is called. Later they are moved to
  // |swapping_presentation_callbacks_| when the effect of the Commit() is
  // scheduled to be drawn and then moved to |swapped_presentation_callbacks_|
  // after receiving VSync parameters update for the previous frame. They fire
  // at the next VSync parameters update after that.
  std::list<PresentationCallback> pending_presentation_callbacks_;

  // This is the state that has yet to be committed.
  State pending_state_;

  // This is the state that has been committed.
  State state_;

  // The stack of sub-surfaces to take effect when Commit() is called.
  // Bottom-most sub-surface at the front of the list and top-most sub-surface
  // at the back.
  using SubSurfaceEntry = std::pair<Surface*, gfx::Point>;
  using SubSurfaceEntryList = std::list<SubSurfaceEntry>;
  SubSurfaceEntryList pending_sub_surfaces_;
  SubSurfaceEntryList sub_surfaces_;

  // The buffer that is currently set as content of surface.
  BufferAttachment current_buffer_;

  // The last resource that was sent to a surface.
  viz::TransferableResource current_resource_;

  // Whether the last resource that was sent to a surface has an alpha channel.
  bool current_resource_has_alpha_ = false;

  // This is true if a call to Commit() as been made but
  // CommitSurfaceHierarchy() has not yet been called.
  bool needs_commit_surface_ = false;

  // This is true if UpdateResources() should be called.
  bool needs_update_resource_ = true;

  // The current buffer transform matrix. It specifies the transformation from
  // normalized buffer coordinates to post-tranform buffer coordinates.
  gfx::Transform buffer_transform_;

  // This is set when the compositing starts and passed to active frame
  // callbacks when compositing successfully ends.
  base::TimeTicks last_compositing_start_time_;

  // Cursor providers. Surface does not own the cursor providers.
  std::set<CursorProvider*> cursor_providers_;

  // This can be set to have some functions delegated. E.g. ShellSurface class
  // can set this to handle Commit() and apply any double buffered state it
  // maintains.
  SurfaceDelegate* delegate_ = nullptr;

  // Surface observer list. Surface does not own the observers.
  base::ObserverList<SurfaceObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_H_
