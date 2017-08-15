// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "components/exo/buffer.h"
#include "components/exo/pointer.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "components/viz/common/quads/single_release_callback.h"
#include "components/viz/common/surfaces/sequence_surface_reference_factory.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(exo::Surface*);

namespace exo {
namespace {

// A property key containing the surface that is associated with
// window. If unset, no surface is associated with window.
DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kSurfaceKey, nullptr);

// A property key to store whether the surface should only consume
// stylus input events.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kStylusOnlyKey, false);

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

// Helper function that returns true if |format| may have an alpha channel.
// Note: False positives are allowed but false negatives are not.
bool FormatHasAlpha(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::UYVY_422:
      return false;
    default:
      return true;
  }
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
    return surface_->GetCursor();
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
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
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
  bool SubtreeCanAcceptEvent(aura::Window* window,
                             const ui::LocatedEvent& event) const override {
    Surface* surface = Surface::AsSurface(window);
    if (!surface)
      return false;

    if (surface->IsStylusOnly()) {
      ui::EventPointerType type = ui::EventPointerType::POINTER_TYPE_UNKNOWN;
      if (event.IsTouchEvent()) {
        auto* touch_event = static_cast<const ui::TouchEvent*>(&event);
        type = touch_event->pointer_details().pointer_type;
      }
      if (type != ui::EventPointerType::POINTER_TYPE_PEN)
        return false;
    }
    return aura::WindowTargeter::SubtreeCanAcceptEvent(window, event);
  }

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

Surface::Surface() : window_(new aura::Window(new CustomWindowDelegate(this))) {
  window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  window_->SetName("ExoSurface");
  window_->SetProperty(kSurfaceKey, this);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->SetEventTargeter(base::WrapUnique(new CustomWindowTargeter));
  window_->set_owned_by_parent(false);
  WMHelper::GetInstance()->SetDragDropDelegate(window_.get());
}
Surface::~Surface() {
  for (SurfaceObserver& observer : observers_)
    observer.OnSurfaceDestroying(this);

  // Call all frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  for (const auto& frame_callback : pending_frame_callbacks_)
    frame_callback.Run(base::TimeTicks());

  // Call all presentation callbacks with a null presentation time to indicate
  // that they have been cancelled.
  for (const auto& presentation_callback : pending_presentation_callbacks_)
    presentation_callback.Run(base::TimeTicks(), base::TimeDelta());

  WMHelper::GetInstance()->ResetDragDropDelegate(window_.get());
}

// static
Surface* Surface::AsSurface(const aura::Window* window) {
  return window->GetProperty(kSurfaceKey);
}

void Surface::Attach(Buffer* buffer) {
  TRACE_EVENT1("exo", "Surface::Attach", "buffer",
               buffer ? buffer->GetSize().ToString() : "null");

  has_pending_contents_ = true;
  pending_buffer_.Reset(buffer ? buffer->AsWeakPtr() : base::WeakPtr<Buffer>());
}

void Surface::Damage(const gfx::Rect& damage) {
  TRACE_EVENT1("exo", "Surface::Damage", "damage", damage.ToString());

  pending_damage_.op(gfx::RectToSkIRect(damage), SkRegion::kUnion_Op);
}

void Surface::RequestFrameCallback(const FrameCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestFrameCallback");

  pending_frame_callbacks_.push_back(callback);
}

void Surface::RequestPresentationCallback(
    const PresentationCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestPresentationCallback");

  pending_presentation_callbacks_.push_back(callback);
}

void Surface::SetOpaqueRegion(const SkRegion& region) {
  TRACE_EVENT1("exo", "Surface::SetOpaqueRegion", "region",
               gfx::SkIRectToRect(region.getBounds()).ToString());

  pending_state_.opaque_region = region;
}

void Surface::SetInputRegion(const SkRegion& region) {
  TRACE_EVENT1("exo", "Surface::SetInputRegion", "region",
               gfx::SkIRectToRect(region.getBounds()).ToString());

  pending_state_.input_region = region;
}

void Surface::SetBufferScale(float scale) {
  TRACE_EVENT1("exo", "Surface::SetBufferScale", "scale", scale);

  pending_state_.buffer_scale = scale;
}

void Surface::AddSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::AddSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  DCHECK(!sub_surface->window()->parent());
  DCHECK(!sub_surface->window()->IsVisible());
  sub_surface->window()->SetBounds(
      gfx::Rect(sub_surface->window()->bounds().size()));
  window_->AddChild(sub_surface->window());

  DCHECK(!ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::Point()));
  sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::Point()));
}

void Surface::RemoveSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::RemoveSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  window_->RemoveChild(sub_surface->window());
  if (sub_surface->window()->IsVisible())
    sub_surface->window()->Hide();

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.erase(
      FindListEntry(pending_sub_surfaces_, sub_surface));
  DCHECK(ListContainsEntry(sub_surfaces_, sub_surface));
  sub_surfaces_.erase(FindListEntry(sub_surfaces_, sub_surface));
}

void Surface::SetSubSurfacePosition(Surface* sub_surface,
                                    const gfx::Point& position) {
  TRACE_EVENT2("exo", "Surface::SetSubSurfacePosition", "sub_surface",
               sub_surface->AsTracedValue(), "position", position.ToString());

  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  DCHECK(it != pending_sub_surfaces_.end());
  if (it->second == position)
    return;
  it->second = position;
  sub_surfaces_changed_ = true;
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
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  if (it == position_it)
    return;
  pending_sub_surfaces_.splice(position_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
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
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  if (it == sibling_it)
    return;
  pending_sub_surfaces_.splice(sibling_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
}

void Surface::SetViewport(const gfx::Size& viewport) {
  TRACE_EVENT1("exo", "Surface::SetViewport", "viewport", viewport.ToString());

  pending_state_.viewport = viewport;
}

void Surface::SetCrop(const gfx::RectF& crop) {
  TRACE_EVENT1("exo", "Surface::SetCrop", "crop", crop.ToString());

  pending_state_.crop = crop;
}

void Surface::SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output) {
  TRACE_EVENT1("exo", "Surface::SetOnlyVisibleOnSecureOutput",
               "only_visible_on_secure_output", only_visible_on_secure_output);

  pending_state_.only_visible_on_secure_output = only_visible_on_secure_output;
}

void Surface::SetBlendMode(SkBlendMode blend_mode) {
  TRACE_EVENT1("exo", "Surface::SetBlendMode", "blend_mode",
               static_cast<int>(blend_mode));

  pending_state_.blend_mode = blend_mode;
}

void Surface::SetAlpha(float alpha) {
  TRACE_EVENT1("exo", "Surface::SetAlpha", "alpha", alpha);

  pending_state_.alpha = alpha;
}

void Surface::Commit() {
  TRACE_EVENT0("exo", "Surface::Commit");

  needs_commit_surface_hierarchy_ = true;
  if (delegate_)
    delegate_->OnSurfaceCommit();
}

void Surface::CommitSurfaceHierarchy(
    const gfx::Point& origin,
    FrameType frame_type,
    LayerTreeFrameSinkHolder* frame_sink_holder,
    cc::CompositorFrame* frame,
    std::list<FrameCallback>* frame_callbacks,
    std::list<PresentationCallback>* presentation_callbacks) {
  bool needs_commit =
      frame_type == FRAME_TYPE_COMMIT && needs_commit_surface_hierarchy_;
  bool needs_full_damage = frame_type == FRAME_TYPE_RECREATED_RESOURCES;

  if (needs_commit) {
    needs_commit_surface_hierarchy_ = false;

    if (pending_state_.opaque_region != state_.opaque_region ||
        pending_state_.buffer_scale != state_.buffer_scale ||
        pending_state_.viewport != state_.viewport ||
        pending_state_.crop != state_.crop ||
        pending_state_.only_visible_on_secure_output !=
            state_.only_visible_on_secure_output ||
        pending_state_.blend_mode != state_.blend_mode ||
        pending_state_.alpha != state_.alpha) {
      needs_full_damage = true;
    }

    state_ = pending_state_;
    pending_state_.only_visible_on_secure_output = false;

    // We update contents if Attach() has been called since last commit.
    if (has_pending_contents_) {
      has_pending_contents_ = false;

      current_buffer_ = std::move(pending_buffer_);

      UpdateResource(frame_sink_holder, true);
    }

    // Move pending frame callbacks to the end of frame_callbacks.
    frame_callbacks->splice(frame_callbacks->end(), pending_frame_callbacks_);

    // Move pending presentation callbacks to the end of presentation_callbacks.
    presentation_callbacks->splice(presentation_callbacks->end(),
                                   pending_presentation_callbacks_);

    UpdateContentSize();

    // Synchronize window hierarchy. This will position and update the stacking
    // order of all sub-surfaces after committing all pending state of
    // sub-surface descendants.
    if (sub_surfaces_changed_) {
      sub_surfaces_.clear();
      aura::Window* stacking_target = nullptr;
      for (const auto& sub_surface_entry : pending_sub_surfaces_) {
        Surface* sub_surface = sub_surface_entry.first;
        sub_surfaces_.push_back(sub_surface_entry);
        // Move sub-surface to its new position in the stack.
        if (stacking_target)
          window_->StackChildAbove(sub_surface->window(), stacking_target);

        // Stack next sub-surface above this sub-surface.
        stacking_target = sub_surface->window();

        // Update sub-surface position relative to surface origin.
        sub_surface->window()->SetBounds(gfx::Rect(
            sub_surface_entry.second, sub_surface->window()->bounds().size()));
      }
      sub_surfaces_changed_ = false;
    }
  }

  // The top most sub-surface is at the front of the RenderPass's quad_list,
  // so we need composite sub-surface in reversed order.
  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    // Synchronsouly commit all pending state of the sub-surface and its
    // decendents.
    sub_surface->CommitSurfaceHierarchy(
        origin + sub_surface_entry.second.OffsetFromOrigin(), frame_type,
        frame_sink_holder, frame, frame_callbacks, presentation_callbacks);
  }

  AppendContentsToFrame(origin, frame, needs_full_damage);

  // Reset damage.
  if (needs_commit)
    pending_damage_.setEmpty();

  DCHECK(
      !current_resource_.id ||
      frame_sink_holder->HasReleaseCallbackForResource(current_resource_.id));
}

bool Surface::IsSynchronized() const {
  return delegate_ ? delegate_->IsSurfaceSynchronized() : false;
}

gfx::Rect Surface::GetHitTestBounds() const {
  SkIRect bounds = state_.input_region.getBounds();
  if (!bounds.intersect(gfx::RectToSkIRect(gfx::Rect(content_size_))))
    return gfx::Rect();
  return gfx::SkIRectToRect(bounds);
}

bool Surface::HitTestRect(const gfx::Rect& rect) const {
  if (HasHitTestMask())
    return state_.input_region.intersects(gfx::RectToSkIRect(rect));

  return rect.Intersects(gfx::Rect(content_size_));
}

bool Surface::HasHitTestMask() const {
  return !state_.input_region.contains(
      gfx::RectToSkIRect(gfx::Rect(content_size_)));
}

void Surface::GetHitTestMask(gfx::Path* mask) const {
  state_.input_region.getBoundaryPath(mask);
}

void Surface::RegisterCursorProvider(Pointer* provider) {
  cursor_providers_.insert(provider);
}

void Surface::UnregisterCursorProvider(Pointer* provider) {
  cursor_providers_.erase(provider);
}

gfx::NativeCursor Surface::GetCursor() {
  // What cursor we display when we have multiple cursor providers is not
  // important. Return the first non-null cursor.
  for (auto* cursor_provider : cursor_providers_) {
    gfx::NativeCursor cursor = cursor_provider->GetCursor();
    if (cursor != ui::CursorType::kNull)
      return cursor;
  }
  return ui::CursorType::kNull;
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
  value->SetString("name", window_->layer()->name());
  return value;
}

bool Surface::IsStylusOnly() {
  return window_->GetProperty(kStylusOnlyKey);
}

void Surface::SetStylusOnly() {
  window_->SetProperty(kStylusOnlyKey, true);
}

void Surface::RecreateResources(LayerTreeFrameSinkHolder* frame_sink_holder) {
  UpdateResource(frame_sink_holder, false);
  for (const auto& sub_surface : sub_surfaces_)
    sub_surface.first->RecreateResources(frame_sink_holder);
}

bool Surface::FillsBoundsOpaquely() const {
  return !current_resource_has_alpha_ ||
         state_.blend_mode == SkBlendMode::kSrc ||
         state_.opaque_region.contains(
             gfx::RectToSkIRect(gfx::Rect(content_size_)));
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

Surface::State::State() : input_region(SkIRect::MakeLargest()) {}

Surface::State::~State() = default;

bool Surface::State::operator==(const State& other) {
  return (other.crop == crop && alpha == other.alpha &&
          other.blend_mode == blend_mode && other.viewport == viewport &&
          other.opaque_region == opaque_region &&
          other.buffer_scale == buffer_scale &&
          other.input_region == input_region);
}

Surface::BufferAttachment::BufferAttachment() {}

Surface::BufferAttachment::~BufferAttachment() {
  if (buffer_)
    buffer_->OnDetach();
}

Surface::BufferAttachment& Surface::BufferAttachment::operator=(
    BufferAttachment&& other) {
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = other.buffer_;
  other.buffer_ = base::WeakPtr<Buffer>();
  return *this;
}

base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() {
  return buffer_;
}

const base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() const {
  return buffer_;
}

void Surface::BufferAttachment::Reset(base::WeakPtr<Buffer> buffer) {
  if (buffer)
    buffer->OnAttach();
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = buffer;
}

void Surface::UpdateResource(LayerTreeFrameSinkHolder* frame_sink_holder,
                             bool client_usage) {
  if (current_buffer_.buffer() &&
      current_buffer_.buffer()->ProduceTransferableResource(
          frame_sink_holder, state_.only_visible_on_secure_output, client_usage,
          &current_resource_)) {
    current_resource_has_alpha_ =
        FormatHasAlpha(current_buffer_.buffer()->GetFormat());
  } else {
    current_resource_.id = 0;
    current_resource_.size = gfx::Size();
    current_resource_has_alpha_ = false;
  }
}

void Surface::AppendContentsToFrame(const gfx::Point& origin,
                                    cc::CompositorFrame* frame,
                                    bool needs_full_damage) {
  const std::unique_ptr<cc::RenderPass>& render_pass =
      frame->render_pass_list.back();
  gfx::Rect output_rect = gfx::Rect(origin, content_size_);
  gfx::Rect quad_rect = output_rect;
  gfx::Rect damage_rect;
  if (needs_full_damage) {
    damage_rect = output_rect;
  } else {
    // pending_damage_ is in Surface coordinates.
    damage_rect = gfx::SkIRectToRect(pending_damage_.getBounds());
    damage_rect.set_origin(origin);
    damage_rect.Intersect(output_rect);
  }

  render_pass->damage_rect.Union(damage_rect);
  cc::SharedQuadState* quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  quad_state->SetAll(
      gfx::Transform() /* quad_to_target_transform */,
      gfx::Rect(content_size_) /* quad_layer_rect */,
      quad_rect /* visible_quad_layer_rect */, gfx::Rect() /* clip_rect */,
      false /* is_clipped */, state_.alpha /* opacity */,
      SkBlendMode::kSrcOver /* blend_mode */, 0 /* sorting_context_id */);

  if (current_resource_.id) {
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(1.f, 1.f);
    if (!state_.crop.IsEmpty()) {
      gfx::SizeF scaled_buffer_size(gfx::ScaleSize(
          gfx::SizeF(current_resource_.size), 1.0f / state_.buffer_scale));
      uv_top_left = state_.crop.origin();

      uv_top_left.Scale(1.f / scaled_buffer_size.width(),
                        1.f / scaled_buffer_size.height());
      uv_bottom_right = state_.crop.bottom_right();
      uv_bottom_right.Scale(1.f / scaled_buffer_size.width(),
                            1.f / scaled_buffer_size.height());
    }
    // Texture quad is only needed if buffer is not fully transparent.
    if (state_.alpha) {
      cc::TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
      float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
      gfx::Rect opaque_rect;
      if (!current_resource_has_alpha_ ||
          state_.blend_mode == SkBlendMode::kSrc ||
          state_.opaque_region.contains(gfx::RectToSkIRect(quad_rect))) {
        opaque_rect = quad_rect;
      } else if (state_.opaque_region.isRect()) {
        opaque_rect = gfx::SkIRectToRect(state_.opaque_region.getBounds());
      }

      texture_quad->SetNew(
          quad_state, quad_rect, opaque_rect, quad_rect, current_resource_.id,
          true /* premultiplied_alpha */, uv_top_left, uv_bottom_right,
          SK_ColorTRANSPARENT /* background_color */, vertex_opacity,
          false /* y_flipped */, false /* nearest_neighbor */,
          state_.only_visible_on_secure_output);
      if (current_resource_.is_overlay_candidate)
        texture_quad->set_resource_size_in_pixels(current_resource_.size);
      frame->resource_list.push_back(current_resource_);
    }
  } else {
    cc::SolidColorDrawQuad* solid_quad =
        render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
    solid_quad->SetNew(quad_state, quad_rect, quad_rect, SK_ColorBLACK,
                       false /* force_anti_aliasing_off */);
  }
}

void Surface::UpdateContentSize() {
  gfx::Size content_size;
  gfx::Size buffer_size = current_resource_.size;
  gfx::SizeF scaled_buffer_size(
      gfx::ScaleSize(gfx::SizeF(buffer_size), 1.0f / state_.buffer_scale));
  if (!state_.viewport.IsEmpty()) {
    content_size = state_.viewport;
  } else if (!state_.crop.IsEmpty()) {
    DLOG_IF(WARNING, !gfx::IsExpressibleAsInt(state_.crop.width()) ||
                         !gfx::IsExpressibleAsInt(state_.crop.height()))
        << "Crop rectangle size (" << state_.crop.size().ToString()
        << ") most be expressible using integers when viewport is not set";
    content_size = gfx::ToCeiledSize(state_.crop.size());
  } else {
    content_size = gfx::ToCeiledSize(scaled_buffer_size);
  }

  // Enable/disable sub-surface based on if it has contents.
  if (has_contents())
    window_->Show();
  else
    window_->Hide();

  if (content_size_ != content_size) {
    content_size_ = content_size;
    window_->SetBounds(gfx::Rect(window_->bounds().origin(), content_size_));
    if (delegate_)
      delegate_->OnSurfaceContentSizeChanged();
  }
}

}  // namespace exo
