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
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/single_release_callback.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "components/exo/buffer.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
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

void SatisfyCallback(cc::SurfaceManager* manager,
                     cc::SurfaceSequence sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.id_namespace, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     cc::SurfaceId id,
                     cc::SurfaceSequence sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

}  // namespace

SurfaceFactoryOwner::SurfaceFactoryOwner() {}
SurfaceFactoryOwner::~SurfaceFactoryOwner() {}

void SurfaceFactoryOwner::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  scoped_refptr<SurfaceFactoryOwner> holder(this);
  for (auto& resource : resources) {
    auto it = release_callbacks_.find(resource.id);
    DCHECK(it != release_callbacks_.end());
    it->second.second->Run(resource.sync_token, resource.lost);
    release_callbacks_.erase(it);
  }
}
void SurfaceFactoryOwner::WillDrawSurface(cc::SurfaceId id,
                                          const gfx::Rect& damage_rect) {
  if (surface_)
    surface_->WillDraw(id);
}

void SurfaceFactoryOwner::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {}

////////////////////////////////////////////////////////////////////////////////
// Surface, public:

Surface::Surface()
    : aura::Window(new CustomWindowDelegate(this)),
      has_pending_contents_(false),
      pending_input_region_(SkIRect::MakeLargest()),
      pending_buffer_scale_(1.0f),
      pending_only_visible_on_secure_output_(false),
      pending_blend_mode_(SkXfermode::kSrcOver_Mode),
      pending_alpha_(1.0f),
      alpha_(0.0f),
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
  surface_manager_ =
      aura::Env::GetInstance()->context_factory()->GetSurfaceManager();
  if (use_surface_layer_) {
    factory_owner_ = make_scoped_refptr(new SurfaceFactoryOwner);
    factory_owner_->surface_ = this;
    factory_owner_->id_allocator_ =
        aura::Env::GetInstance()->context_factory()->CreateSurfaceIdAllocator();
    factory_owner_->surface_factory_.reset(
        new cc::SurfaceFactory(surface_manager_, factory_owner_.get()));
  }

  if (!factory_owner_) {
    AddObserver(this);
  }
}

Surface::~Surface() {
  FOR_EACH_OBSERVER(SurfaceObserver, observers_, OnSurfaceDestroying(this));

  layer()->SetShowSolidColorContent();

  if (factory_owner_) {
    factory_owner_->surface_ = nullptr;
  } else {
    RemoveObserver(this);
  }
  if (compositor_)
    compositor_->RemoveObserver(this);

  // Call pending frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
  for (const auto& frame_callback : active_frame_callbacks_)
    frame_callback.Run(base::TimeTicks());

  if (!surface_id_.is_null())
    factory_owner_->surface_factory_->Destroy(surface_id_);
}

// static
Surface* Surface::AsSurface(const aura::Window* window) {
  return window->GetProperty(kSurfaceKey);
}

// static
void Surface::SetUseSurfaceLayer(bool use_surface_layer) {
  use_surface_layer_ = use_surface_layer;
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

void Surface::SetCrop(const gfx::RectF& crop) {
  TRACE_EVENT1("exo", "Surface::SetCrop", "crop", crop.ToString());

  pending_crop_ = crop;
}

void Surface::SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output) {
  TRACE_EVENT1("exo", "Surface::SetOnlyVisibleOnSecureOutput",
               "only_visible_on_secure_output", only_visible_on_secure_output);

  pending_only_visible_on_secure_output_ = only_visible_on_secure_output;
}

void Surface::SetBlendMode(SkXfermode::Mode blend_mode) {
  TRACE_EVENT1("exo", "Surface::SetBlendMode", "blend_mode", blend_mode);

  pending_blend_mode_ = blend_mode;
}

void Surface::SetAlpha(float alpha) {
  TRACE_EVENT1("exo", "Surface::SetAlpha", "alpha", alpha);

  pending_alpha_ = alpha;
}

void Surface::Commit() {
  TRACE_EVENT0("exo", "Surface::Commit");

  needs_commit_surface_hierarchy_ = true;

  if (delegate_)
    delegate_->OnSurfaceCommit();
  else
    CommitSurfaceHierarchy();
}

void Surface::CommitLayerContents() {
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

    // Update layer with the new contents.
    if (texture_mailbox_release_callback) {
      gfx::Size texture_size_in_dip = gfx::ScaleToFlooredSize(
          texture_mailbox.size_in_pixels(), 1.0f / pending_buffer_scale_);
      // Determine the new surface size.
      // - Texture size in DIP defines the size if nothing else is set.
      // - If a viewport is set then that defines the size, otherwise
      //   the crop rectangle defines the size if set.
      gfx::Size contents_size = texture_size_in_dip;
      if (!pending_viewport_.IsEmpty()) {
        contents_size = pending_viewport_;
      } else if (!pending_crop_.IsEmpty()) {
        DLOG_IF(WARNING, !gfx::IsExpressibleAsInt(pending_crop_.width()) ||
                             !gfx::IsExpressibleAsInt(pending_crop_.height()))
            << "Crop rectangle size (" << pending_crop_.size().ToString()
            << ") most be expressible using integers when viewport is not set";
        contents_size = gfx::ToCeiledSize(pending_crop_.size());
      }
      layer()->SetTextureMailbox(texture_mailbox,
                                 std::move(texture_mailbox_release_callback),
                                 texture_size_in_dip);
      layer()->SetTextureFlipped(false);
      layer()->SetTextureCrop(pending_crop_);
      layer()->SetTextureScale(
          static_cast<float>(texture_size_in_dip.width()) /
              contents_size.width(),
          static_cast<float>(texture_size_in_dip.height()) /
              contents_size.height());
      layer()->SetBounds(gfx::Rect(layer()->bounds().origin(), contents_size));
    } else {
      // Show solid color content if no buffer is attached or we failed
      // to produce a texture mailbox for the currently attached buffer.
      layer()->SetShowSolidColorContent();
      layer()->SetColor(SK_ColorBLACK);
      alpha_ = 1.0f;
    }

    // Schedule redraw of the damage region.
    for (SkRegion::Iterator it(pending_damage_); !it.done(); it.next())
      layer()->SchedulePaint(gfx::SkIRectToRect(it.rect()));

    // Reset damage.
    pending_damage_.setEmpty();
  }

  if (layer()->has_external_content()) {
    layer()->SetTextureAlpha(pending_alpha_);
    alpha_ = pending_alpha_;
  }

  // Move pending frame callbacks to the end of |frame_callbacks_|.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);

  // Update alpha compositing properties.
  // TODO(reveman): Use a more reliable way to force blending off than setting
  // fills-bounds-opaquely.
  layer()->SetFillsBoundsOpaquely(
      pending_blend_mode_ == SkXfermode::kSrc_Mode ||
      pending_opaque_region_.contains(
          gfx::RectToSkIRect(gfx::Rect(layer()->size()))));
}

void Surface::CommitSurfaceContents() {
  // We update contents if Attach() has been called since last commit.
  if (has_pending_contents_) {
    has_pending_contents_ = false;

    current_buffer_ = pending_buffer_;
    pending_buffer_.reset();

    bool secure_output_only = pending_only_visible_on_secure_output_;
    pending_only_visible_on_secure_output_ = false;

    cc::TextureMailbox texture_mailbox;
    std::unique_ptr<cc::SingleReleaseCallback> texture_mailbox_release_callback;
    if (current_buffer_) {
      texture_mailbox_release_callback = current_buffer_->ProduceTextureMailbox(
          &texture_mailbox, secure_output_only, false);
    }

    cc::SurfaceId old_surface_id = surface_id_;
    surface_id_ = factory_owner_->id_allocator_->GenerateId();
    factory_owner_->surface_factory_->Create(surface_id_);

    gfx::Size buffer_size = texture_mailbox.size_in_pixels();
    gfx::SizeF scaled_buffer_size(
        gfx::ScaleSize(gfx::SizeF(buffer_size), 1.0f / pending_buffer_scale_));

    gfx::Size layer_size;  // Size of the output layer, in DIP.
    if (!pending_viewport_.IsEmpty()) {
      layer_size = pending_viewport_;
    } else if (!pending_crop_.IsEmpty()) {
      DLOG_IF(WARNING, !gfx::IsExpressibleAsInt(pending_crop_.width()) ||
                           !gfx::IsExpressibleAsInt(pending_crop_.height()))
          << "Crop rectangle size (" << pending_crop_.size().ToString()
          << ") most be expressible using integers when viewport is not set";
      layer_size = gfx::ToCeiledSize(pending_crop_.size());
    } else {
      layer_size = gfx::ToCeiledSize(scaled_buffer_size);
    }

    // TODO(jbauman): Figure out how this interacts with the pixel size of
    // CopyOutputRequests on the layer.
    float contents_surface_to_layer_scale = 1.0;
    gfx::Size contents_surface_size = layer_size;

    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(1.f, 1.f);
    if (!pending_crop_.IsEmpty()) {
      uv_top_left = pending_crop_.origin();

      uv_top_left.Scale(1.f / scaled_buffer_size.width(),
                        1.f / scaled_buffer_size.height());
      uv_bottom_right = pending_crop_.bottom_right();
      uv_bottom_right.Scale(1.f / scaled_buffer_size.width(),
                            1.f / scaled_buffer_size.height());
    }

    // pending_damage_ is in Surface coordinates.
    gfx::Rect damage_rect = gfx::SkIRectToRect(pending_damage_.getBounds());

    std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
    render_pass->SetAll(cc::RenderPassId(1, 1),
                        gfx::Rect(contents_surface_size), damage_rect,
                        gfx::Transform(), false);

    gfx::Rect quad_rect = gfx::Rect(contents_surface_size);
    cc::SharedQuadState* quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    quad_state->quad_layer_bounds = contents_surface_size;
    quad_state->visible_quad_layer_rect = quad_rect;
    quad_state->opacity = alpha_;
    alpha_ = pending_alpha_;

    bool frame_is_opaque = false;

    std::unique_ptr<cc::DelegatedFrameData> delegated_frame(
        new cc::DelegatedFrameData);
    if (texture_mailbox_release_callback) {
      cc::TransferableResource resource;
      resource.id = next_resource_id_++;
      resource.format = cc::RGBA_8888;
      resource.filter =
          texture_mailbox.nearest_neighbor() ? GL_NEAREST : GL_LINEAR;
      resource.size = texture_mailbox.size_in_pixels();
      resource.mailbox_holder = gpu::MailboxHolder(texture_mailbox.mailbox(),
                                                   texture_mailbox.sync_token(),
                                                   texture_mailbox.target());
      resource.is_overlay_candidate = texture_mailbox.is_overlay_candidate();

      cc::TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
      float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
      gfx::Rect opaque_rect;
      frame_is_opaque =
          pending_blend_mode_ == SkXfermode::kSrc_Mode ||
          pending_opaque_region_.contains(gfx::RectToSkIRect(quad_rect));
      if (frame_is_opaque) {
        opaque_rect = quad_rect;
      } else if (pending_opaque_region_.isRect()) {
        opaque_rect = gfx::SkIRectToRect(pending_opaque_region_.getBounds());
      }

      texture_quad->SetNew(quad_state, quad_rect, opaque_rect, quad_rect,
                           resource.id, true, uv_top_left, uv_bottom_right,
                           SK_ColorTRANSPARENT, vertex_opacity, false, false,
                           secure_output_only);

      factory_owner_->release_callbacks_[resource.id] = std::make_pair(
          factory_owner_, std::move(texture_mailbox_release_callback));
      delegated_frame->resource_list.push_back(resource);
    } else {
      cc::SolidColorDrawQuad* solid_quad =
          render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
      solid_quad->SetNew(quad_state, quad_rect, quad_rect, SK_ColorBLACK,
                         false);
      frame_is_opaque = true;
    }

    delegated_frame->render_pass_list.push_back(std::move(render_pass));
    std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
    frame->delegated_frame_data = std::move(delegated_frame);

    factory_owner_->surface_factory_->SubmitCompositorFrame(
        surface_id_, std::move(frame), cc::SurfaceFactory::DrawCallback());

    if (!old_surface_id.is_null()) {
      factory_owner_->surface_factory_->SetPreviousFrameSurface(surface_id_,
                                                                old_surface_id);
      factory_owner_->surface_factory_->Destroy(old_surface_id);
    }

    layer()->SetShowSurface(
        surface_id_,
        base::Bind(&SatisfyCallback, base::Unretained(surface_manager_)),
        base::Bind(&RequireCallback, base::Unretained(surface_manager_)),
        contents_surface_size, contents_surface_to_layer_scale, layer_size);
    layer()->SetBounds(gfx::Rect(layer()->bounds().origin(), layer_size));
    layer()->SetFillsBoundsOpaquely(alpha_ == 1.0f && frame_is_opaque);

    // Reset damage.
    pending_damage_.setEmpty();
  }
  // Move pending frame callbacks to the end of active_frame_callbacks_
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 pending_frame_callbacks_);
}

void Surface::CommitSurfaceHierarchy() {
  DCHECK(needs_commit_surface_hierarchy_);
  needs_commit_surface_hierarchy_ = false;

  if (factory_owner_) {
    CommitSurfaceContents();
  } else {
    CommitLayerContents();
  }

  // Update current input region.
  input_region_ = pending_input_region_;

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

gfx::Rect Surface::GetNonTransparentBounds() const {
  gfx::Rect non_transparent_bounds;
  if (alpha_)
    non_transparent_bounds = gfx::Rect(layer()->size());

  for (auto& sub_surface_entry : pending_sub_surfaces_) {
    Surface* sub_surface = sub_surface_entry.first;
    if (!sub_surface->has_contents())
      continue;
    non_transparent_bounds.Union(sub_surface->GetNonTransparentBounds() +
                                 sub_surface->bounds().OffsetFromOrigin());
  }

  return non_transparent_bounds;
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
  DCHECK(!factory_owner_);
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
  DCHECK(!factory_owner_);
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

void Surface::WillDraw(cc::SurfaceId id) {
  while (!active_frame_callbacks_.empty()) {
    active_frame_callbacks_.front().Run(base::TimeTicks::Now());
    active_frame_callbacks_.pop_front();
  }
}

bool Surface::use_surface_layer_ = false;

}  // namespace exo
