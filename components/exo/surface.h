// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_H_
#define COMPONENTS_EXO_SURFACE_H_

#include <list>
#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "cc/resources/transferable_resource.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "components/exo/compositor_frame_sink.h"
#include "components/exo/compositor_frame_sink_holder.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace cc {
class LocalSurfaceIdAllocator;
}

namespace gfx {
class Path;
}

namespace exo {
class Buffer;
class Pointer;
class SurfaceDelegate;
class SurfaceObserver;
class Surface;

namespace subtle {
class PropertyHelper;
}

// The pointer class is currently the only cursor provider class but this can
// change in the future when better hardware cursor support is added.
using CursorProvider = Pointer;

// This class represents a rectangular area that is displayed on the screen.
// It has a location, size and pixel contents.
class Surface : public ui::ContextFactoryObserver,
                public aura::WindowObserver,
                public ui::PropertyHandler,
                public ui::CompositorVSyncManager::Observer {
 public:
  using PropertyDeallocator = void (*)(int64_t value);

  Surface();
  ~Surface() override;

  // Type-checking downcast routine.
  static Surface* AsSurface(const aura::Window* window);

  aura::Window* window() { return window_.get(); }

  cc::SurfaceId GetSurfaceId() const;

  CompositorFrameSinkHolder* compositor_frame_sink_holder() {
    return compositor_frame_sink_holder_.get();
  }

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

  // Surface state (damage regions, attached buffers, etc.) is double-buffered.
  // A Commit() call atomically applies all pending state, replacing the
  // current state. Commit() is not guaranteed to be synchronous. See
  // CommitSurfaceHierarchy() below.
  void Commit();

  // This will synchronously commit all pending state of the surface and its
  // descendants by recursively calling CommitSurfaceHierarchy() for each
  // sub-surface with pending state.
  void CommitSurfaceHierarchy();

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

  // Surface does not own cursor providers. It is the responsibility of the
  // caller to remove the cursor provider before it is destroyed.
  void RegisterCursorProvider(CursorProvider* provider);
  void UnregisterCursorProvider(CursorProvider* provider);

  // Returns the cursor for the surface. If no cursor provider is registered
  // then kCursorNull is returned.
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

  // Call this to indicate that surface is being scheduled for a draw.
  void WillDraw();

  // Returns true when there's an active frame callback that requires a
  // BeginFrame() call.
  bool NeedsBeginFrame() const;

  // Call this to indicate that it's a good time to start producing a new frame.
  void BeginFrame(base::TimeTicks frame_time);

  // Check whether this Surface and its children need to create new cc::Surface
  // IDs for their contents next time they get new buffer contents.
  void CheckIfSurfaceHierarchyNeedsCommitToNewSurfaces();

  // Returns the active contents size.
  gfx::Size content_size() const { return content_size_; }

  // Overridden from ui::ContextFactoryObserver:
  void OnLostResources() override;

  // Overridden from aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // Overridden from ui::CompositorVSyncManager::Observer:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

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

  bool needs_commit_surface_hierarchy() const {
    return needs_commit_surface_hierarchy_;
  }

  // Returns true if this surface or any child surface needs a commit and has
  // has_pending_layer_changes_ true.
  bool HasLayerHierarchyChanged() const;

  // Sets that all children must create new cc::SurfaceIds for their contents.
  void SetSurfaceHierarchyNeedsCommitToNewSurfaces();

  // Set SurfaceLayer contents to the current buffer.
  void SetSurfaceLayerContents(ui::Layer* layer);

  // Updates current_resource_ with a new resource id corresponding to the
  // contents of the attached buffer (or id 0, if no buffer is attached).
  // UpdateSurface must be called afterwards to ensure the release callback
  // will be called.
  void UpdateResource(bool client_usage);

  // Updates the current Surface with a new frame referring to the resource in
  // current_resource_.
  void UpdateSurface(bool full_damage);

  // This returns true when the surface has some contents assigned to it.
  bool has_contents() const { return !!current_buffer_.buffer(); }

  // This window has the layer which contains the Surface contents.
  std::unique_ptr<aura::Window> window_;

  // This is true if it's possible that the layer properties (size, opacity,
  // etc.) may have been modified since the last commit. Attaching a new
  // buffer with the same size as the old shouldn't set this to true.
  bool has_pending_layer_changes_ = true;

  // This is true if the next commit to this surface should put its contents
  // into a new cc::SurfaceId. This allows for synchronization between Surface
  // and layer changes.
  bool needs_commit_to_new_surface_ = true;

  // This is the size of the last committed contents.
  gfx::Size content_size_;

  // This is true when Attach() has been called and new contents should take
  // effect next time Commit() is called.
  bool has_pending_contents_ = false;

  // The buffer that will become the content of surface when Commit() is called.
  BufferAttachment pending_buffer_;

  const cc::FrameSinkId frame_sink_id_;
  cc::LocalSurfaceId local_surface_id_;

  scoped_refptr<CompositorFrameSinkHolder> compositor_frame_sink_holder_;

  cc::LocalSurfaceIdAllocator id_allocator_;

  // The next resource id the buffer will be attached to.
  int next_resource_id_ = 1;

  // The damage region to schedule paint for when Commit() is called.
  SkRegion pending_damage_;

  // These lists contains the callbacks to notify the client when it is a good
  // time to start producing a new frame. These callbacks move to
  // |frame_callbacks_| when Commit() is called. Later they are moved to
  // |active_frame_callbacks_| when the effect of the Commit() is scheduled to
  // be drawn. They fire at the first begin frame notification after this.
  std::list<FrameCallback> pending_frame_callbacks_;
  std::list<FrameCallback> frame_callbacks_;
  std::list<FrameCallback> active_frame_callbacks_;

  // These lists contains the callbacks to notify the client when surface
  // contents have been presented. These callbacks move to
  // |presentation_callbacks_| when Commit() is called. Later they are moved to
  // |swapping_presentation_callbacks_| when the effect of the Commit() is
  // scheduled to be drawn and then moved to |swapped_presentation_callbacks_|
  // after receiving VSync parameters update for the previous frame. They fire
  // at the next VSync parameters update after that.
  std::list<PresentationCallback> pending_presentation_callbacks_;
  std::list<PresentationCallback> presentation_callbacks_;
  std::list<PresentationCallback> swapping_presentation_callbacks_;
  std::list<PresentationCallback> swapped_presentation_callbacks_;

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

  // The buffer that is currently set as content of surface.
  BufferAttachment current_buffer_;

  // The last resource that was sent to a surface.
  cc::TransferableResource current_resource_;

  // Whether the last resource that was sent to a surface has an alpha channel.
  bool current_resource_has_alpha_ = false;

  // This is true if a call to Commit() as been made but
  // CommitSurfaceHierarchy() has not yet been called.
  bool needs_commit_surface_hierarchy_ = false;

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

  // A reference factory that uses the compositor frame sink holder provided
  // to this class to construct surface references. This object is passed to
  // ui::Layer::SetShowSurface because the layer needs to know how to add
  // references to surfaces.
  scoped_refptr<cc::SurfaceReferenceFactory> surface_reference_factory_;

  DISALLOW_COPY_AND_ASSIGN(Surface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_H_
