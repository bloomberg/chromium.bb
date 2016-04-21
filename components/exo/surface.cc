// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/resources/single_release_callback.h"
#include "components/exo/buffer.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/path.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

DECLARE_WINDOW_PROPERTY_TYPE(exo::Surface*);

namespace exo {
namespace {

// A property key containing the surface that is associated with
// window. If unset, no surface is associated with window.
DEFINE_WINDOW_PROPERTY_KEY(Surface*, kSurfaceKey, nullptr);

// Helper function that returns an iterator to the first entry in |list|
// with |key|.
template <typename T, typename U>
typename T::iterator FindListEntry(T& list, U key) {
  return std::find_if(list.begin(), list.end(),
                      [key](const typename T::value_type& entry) {
                        return entry.first == key;
                      });
}

// Helper function that returns true if |list| contains an entry with |key|.
template <typename T, typename U>
bool ListContainsEntry(T& list, U key) {
  return FindListEntry(list, key) != list.end();
}

class CustomWindowDelegate : public aura::WindowDelegate {
 public:
  explicit CustomWindowDelegate(Surface* surface) : surface_(surface) {}
  ~CustomWindowDelegate() override {}

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return ui::kCursorNone;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return surface_->HasHitTestMask(); }
  void GetHitTestMask(gfx::Path* mask) const override {
    surface_->GetHitTestMask(mask);
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Propagates the key event upto the top-level views Widget so that we can
    // trigger proper events in the views/ash level there. Event handling for
    // Surfaces is done in a post event handler in keyboard.cc.
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_);
    if (widget)
      widget->OnKeyEvent(event);
  }

 private:
  Surface* const surface_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowDelegate);
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter() {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = Surface::AsSurface(window);
    if (!surface)
      return false;

    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Surface, public:

Surface::Surface()
    : aura::Window(new CustomWindowDelegate(this)),
      has_pending_contents_(false),
      pending_input_region_(SkIRect::MakeLargest()),
      pending_buffer_scale_(1.0f),
      pending_only_visible_on_secure_output_(false),
      input_region_(SkIRect::MakeLargest()),
      needs_commit_surface_hierarchy_(false),
      update_contents_after_successful_compositing_(false),
      compositor_(nullptr),
      delegate_(nullptr) {
  SetType(ui::wm::WINDOW_TYPE_CONTROL);
  SetName("ExoSurface");
  SetProperty(kSurfaceKey, this);
  Init(ui::LAYER_SOLID_COLOR);
  SetEventTargeter(base::WrapUnique(new CustomWindowTargeter));
  set_owned_by_parent(false);
  AddObserver(this);
}

Surface::~Surface() {
  FOR_EACH_OBSERVER(SurfaceObserver, observers_, OnSurfaceDestroying(this));

  layer()->SetShowSolidColorContent();

  RemoveObserver(this);
  if (compositor_)
    compositor_->RemoveObserver(this);

  // Call pending frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
  for (const auto& frame_callback : active_frame_callbacks_)
    frame_callback.Run(base::TimeTicks());
}

// static
Surface* Surface::AsSurface(const aura::Window* window) {
  return window->GetProperty(kSurfaceKey);
}

void Surface::Attach(Buffer* buffer) {
  TRACE_EVENT1("exo", "Surface::Attach", "buffer",
               buffer ? buffer->GetSize().ToString() : "null");

  has_pending_contents_ = true;
  pending_buffer_ = buffer ? buffer->AsWeakPtr() : base::WeakPtr<Buffer>();
}

void Surface::Damage(const gfx::Rect& damage) {
  TRACE_EVENT1("exo", "Surface::Damage", "damage", damage.ToString());

  pending_damage_.op(gfx::RectToSkIRect(damage), SkRegion::kUnion_Op);
}

void Surface::RequestFrameCallback(const FrameCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestFrameCallback");

  pending_frame_callbacks_.push_back(callback);
}

void Surface::SetOpaqueRegion(const SkRegion& region) {
  TRACE_EVENT1("exo", "Surface::SetOpaqueRegion", "region",
               gfx::SkIRectToRect(region.getBounds()).ToString());

  pending_opaque_region_ = region;
}

void Surface::SetInputRegion(const SkRegion& region) {
  TRACE_EVENT1("exo", "Surface::SetInputRegion", "region",
               gfx::SkIRectToRect(region.getBounds()).ToString());

  pending_input_region_ = region;
}

void Surface::SetBufferScale(float scale) {
  TRACE_EVENT1("exo", "Surface::SetBufferScale", "scale", scale);

  pending_buffer_scale_ = scale;
}

void Surface::AddSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::AddSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  DCHECK(!sub_surface->parent());
  DCHECK(!sub_surface->IsVisible());
  AddChild(sub_surface);

  DCHECK(!ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::Point()));
}

void Surface::RemoveSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::AddSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  RemoveChild(sub_surface);
  if (sub_surface->IsVisible())
    sub_surface->Hide();

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.erase(
      FindListEntry(pending_sub_surfaces_, sub_surface));
}

void Surface::SetSubSurfacePosition(Surface* sub_surface,
                                    const gfx::Point& position) {
  TRACE_EVENT2("exo", "Surface::SetSubSurfacePosition", "sub_surface",
               sub_surface->AsTracedValue(), "position", position.ToString());

  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  DCHECK(it != pending_sub_surfaces_.end());
  it->second = position;
}

void Surface::PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceAbove", "sub_surface",
               sub_surface->AsTracedValue(), "reference",
               reference->AsTracedValue());

  if (sub_surface == reference) {
    DLOG(WARNING) << "Client tried to place sub-surface above itself";
    return;
  }

  auto position_it = pending_sub_surfaces_.begin();
  if (reference != this) {
    position_it = FindListEntry(pending_sub_surfaces_, reference);
    if (position_it == pending_sub_surfaces_.end()) {
      DLOG(WARNING) << "Client tried to place sub-surface above a reference "
                       "surface that is neither a parent nor a sibling";
      return;
    }

    // Advance iterator to have |position_it| point to the sibling surface
    // above |reference|.
    ++position_it;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.splice(
      position_it, pending_sub_surfaces_,
      FindListEntry(pending_sub_surfaces_, sub_surface));
}

void Surface::PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceBelow", "sub_surface",
               sub_surface->AsTracedValue(), "sibling",
               sibling->AsTracedValue());

  if (sub_surface == sibling) {
    DLOG(WARNING) << "Client tried to place sub-surface below itself";
    return;
  }

  auto sibling_it = FindListEntry(pending_sub_surfaces_, sibling);
  if (sibling_it == pending_sub_surfaces_.end()) {
    DLOG(WARNING) << "Client tried to place sub-surface below a surface that "
                     "is not a sibling";
    return;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.splice(
      sibling_it, pending_sub_surfaces_,
      FindListEntry(pending_sub_surfaces_, sub_surface));
}

void Surface::SetViewport(const gfx::Size& viewport) {
  TRACE_EVENT1("exo", "Surface::SetViewport", "viewport", viewport.ToString());

  pending_viewport_ = viewport;
}

void Surface::SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output) {
  TRACE_EVENT1("exo", "Surface::SetOnlyVisibleOnSecureOutput",
               "only_visible_on_secure_output", only_visible_on_secure_output);

  pending_only_visible_on_secure_output_ = only_visible_on_secure_output;
}

void Surface::Commit() {
  TRACE_EVENT0("exo", "Surface::Commit");

  needs_commit_surface_hierarchy_ = true;

  if (delegate_)
    delegate_->OnSurfaceCommit();
  else
    CommitSurfaceHierarchy();
}

void Surface::CommitSurfaceHierarchy() {
  DCHECK(needs_commit_surface_hierarchy_);
  needs_commit_surface_hierarchy_ = false;

  // We update contents if Attach() has been called since last commit.
  if (has_pending_contents_) {
    has_pending_contents_ = false;

    current_buffer_ = pending_buffer_;
    pending_buffer_.reset();

    // TODO(dcastagna): Make secure_output_only a layer property instead of a
    // texture mailbox flag so this can be changed without have to provide
    // new contents.
    bool secure_output_only = pending_only_visible_on_secure_output_;
    pending_only_visible_on_secure_output_ = false;

    cc::TextureMailbox texture_mailbox;
    std::unique_ptr<cc::SingleReleaseCallback> texture_mailbox_release_callback;
    if (current_buffer_) {
      texture_mailbox_release_callback = current_buffer_->ProduceTextureMailbox(
          &texture_mailbox, secure_output_only, false);
    }

    if (texture_mailbox_release_callback) {
      // Update layer with the new contents. If a viewport has been set then
      // use that to determine the size of the layer and the surface, otherwise
      // buffer scale and buffer size determines the size.
      gfx::Size contents_size =
          pending_viewport_.IsEmpty()
              ? gfx::ScaleToFlooredSize(texture_mailbox.size_in_pixels(),
                                        1.0f / pending_buffer_scale_)
              : pending_viewport_;
      layer()->SetTextureMailbox(texture_mailbox,
                                 std::move(texture_mailbox_release_callback),
                                 contents_size);
      layer()->SetTextureFlipped(false);
      layer()->SetBounds(gfx::Rect(layer()->bounds().origin(), contents_size));
      layer()->SetFillsBoundsOpaquely(pending_opaque_region_.contains(
          gfx::RectToSkIRect(gfx::Rect(contents_size))));
    } else {
      // Show solid color content if no buffer is attached or we failed
      // to produce a texture mailbox for the currently attached buffer.
      layer()->SetShowSolidColorContent();
      layer()->SetColor(SK_ColorBLACK);
    }

    // Schedule redraw of the damage region.
    for (SkRegion::Iterator it(pending_damage_); !it.done(); it.next())
      layer()->SchedulePaint(gfx::SkIRectToRect(it.rect()));

    // Reset damage.
    pending_damage_.setEmpty();
  }

  // Update current input region.
  input_region_ = pending_input_region_;

  // Move pending frame callbacks to the end of |frame_callbacks_|.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);

  // Synchronize window hierarchy. This will position and update the stacking
  // order of all sub-surfaces after committing all pending state of sub-surface
  // descendants.
  aura::Window* stacking_target = nullptr;
  for (auto& sub_surface_entry : pending_sub_surfaces_) {
    Surface* sub_surface = sub_surface_entry.first;

    // Synchronsouly commit all pending state of the sub-surface and its
    // decendents.
    if (sub_surface->needs_commit_surface_hierarchy())
      sub_surface->CommitSurfaceHierarchy();

    // Enable/disable sub-surface based on if it has contents.
    if (sub_surface->has_contents())
      sub_surface->Show();
    else
      sub_surface->Hide();

    // Move sub-surface to its new position in the stack.
    if (stacking_target)
      StackChildAbove(sub_surface, stacking_target);

    // Stack next sub-surface above this sub-surface.
    stacking_target = sub_surface;

    // Update sub-surface position relative to surface origin.
    sub_surface->SetBounds(
        gfx::Rect(sub_surface_entry.second, sub_surface->layer()->size()));
  }
}

bool Surface::IsSynchronized() const {
  return delegate_ ? delegate_->IsSurfaceSynchronized() : false;
}

gfx::Rect Surface::GetHitTestBounds() const {
  SkIRect bounds = input_region_.getBounds();
  if (!bounds.intersect(gfx::RectToSkIRect(gfx::Rect(layer()->size()))))
    return gfx::Rect();
  return gfx::SkIRectToRect(bounds);
}

bool Surface::HitTestRect(const gfx::Rect& rect) const {
  if (HasHitTestMask())
    return input_region_.intersects(gfx::RectToSkIRect(rect));

  return rect.Intersects(gfx::Rect(layer()->size()));
}

bool Surface::HasHitTestMask() const {
  return !input_region_.contains(
      gfx::RectToSkIRect(gfx::Rect(layer()->size())));
}

void Surface::GetHitTestMask(gfx::Path* mask) const {
  input_region_.getBoundaryPath(mask);
}

void Surface::SetSurfaceDelegate(SurfaceDelegate* delegate) {
  DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
}

bool Surface::HasSurfaceDelegate() const {
  return !!delegate_;
}

void Surface::AddSurfaceObserver(SurfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void Surface::RemoveSurfaceObserver(SurfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Surface::HasSurfaceObserver(const SurfaceObserver* observer) const {
  return observers_.HasObserver(observer);
}

std::unique_ptr<base::trace_event::TracedValue> Surface::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("name", layer()->name());
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void Surface::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK(!compositor_);
  compositor_ = layer()->GetCompositor();
  compositor_->AddObserver(this);
}

void Surface::OnWindowRemovingFromRootWindow(aura::Window* window,
                                             aura::Window* new_root) {
  DCHECK(compositor_);
  compositor_->RemoveObserver(this);
  compositor_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorObserver overrides:

void Surface::OnCompositingDidCommit(ui::Compositor* compositor) {
  // Move frame callbacks to the end of |active_frame_callbacks_|.
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
}

void Surface::OnCompositingStarted(ui::Compositor* compositor,
                                   base::TimeTicks start_time) {
  last_compositing_start_time_ = start_time;
}

void Surface::OnCompositingEnded(ui::Compositor* compositor) {
  // Run all frame callbacks associated with the compositor's active tree.
  while (!active_frame_callbacks_.empty()) {
    active_frame_callbacks_.front().Run(last_compositing_start_time_);
    active_frame_callbacks_.pop_front();
  }

  // Nothing more to do in here unless this has been set.
  if (!update_contents_after_successful_compositing_)
    return;

  update_contents_after_successful_compositing_ = false;

  // Early out if no contents is currently assigned to the surface.
  if (!current_buffer_)
    return;

  // TODO(dcastagna): Make secure_output_only a layer property instead of a
  // texture mailbox flag.
  bool secure_output_only = false;

  // Update contents by producing a new texture mailbox for the current buffer.
  cc::TextureMailbox texture_mailbox;
  std::unique_ptr<cc::SingleReleaseCallback> texture_mailbox_release_callback =
      current_buffer_->ProduceTextureMailbox(&texture_mailbox,
                                             secure_output_only, true);
  if (texture_mailbox_release_callback) {
    layer()->SetTextureMailbox(texture_mailbox,
                               std::move(texture_mailbox_release_callback),
                               layer()->bounds().size());
    layer()->SetTextureFlipped(false);
    layer()->SchedulePaint(gfx::Rect(texture_mailbox.size_in_pixels()));
  }
}

void Surface::OnCompositingAborted(ui::Compositor* compositor) {
  // The contents of this surface might be lost if compositing aborted because
  // of a lost graphics context. We recover from this by updating the contents
  // of the surface next time the compositor successfully ends compositing.
  update_contents_after_successful_compositing_ = true;
}

void Surface::OnCompositingShuttingDown(ui::Compositor* compositor) {
  compositor->RemoveObserver(this);
  compositor_ = nullptr;
}

}  // namespace exo
