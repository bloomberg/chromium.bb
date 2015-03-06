// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/animation/scrollbar_animation_controller_linear_fade.h"
#include "cc/animation/scrollbar_animation_controller_thinning.h"
#include "cc/base/math_util.h"
#include "cc/base/synced_property.h"
#include "cc/base/util.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

// This class exists to split the LayerScrollOffsetDelegate between the
// InnerViewportScrollLayer and the OuterViewportScrollLayer in a manner
// that never requires the embedder or LayerImpl to know about.
class LayerScrollOffsetDelegateProxy : public LayerImpl::ScrollOffsetDelegate {
 public:
  LayerScrollOffsetDelegateProxy(LayerImpl* layer,
                                 LayerScrollOffsetDelegate* delegate,
                                 LayerTreeImpl* layer_tree)
      : layer_(layer), delegate_(delegate), layer_tree_impl_(layer_tree) {}
  virtual ~LayerScrollOffsetDelegateProxy() {}

  gfx::ScrollOffset last_set_scroll_offset() const {
    return last_set_scroll_offset_;
  }

  // LayerScrollOffsetDelegate implementation.
  void SetCurrentScrollOffset(const gfx::ScrollOffset& new_offset) override {
    last_set_scroll_offset_ = new_offset;
  }

  gfx::ScrollOffset GetCurrentScrollOffset() override {
    return layer_tree_impl_->GetDelegatedScrollOffset(layer_);
  }

  bool IsExternalFlingActive() const override {
    return delegate_->IsExternalFlingActive();
  }

  void Update() const override {
    layer_tree_impl_->UpdateScrollOffsetDelegate();
  }

 private:
  LayerImpl* layer_;
  LayerScrollOffsetDelegate* delegate_;
  LayerTreeImpl* layer_tree_impl_;
  gfx::ScrollOffset last_set_scroll_offset_;
};

LayerTreeImpl::LayerTreeImpl(
    LayerTreeHostImpl* layer_tree_host_impl,
    scoped_refptr<SyncedProperty<ScaleGroup>> page_scale_factor,
    scoped_refptr<SyncedTopControls> top_controls_shown_ratio,
    scoped_refptr<SyncedElasticOverscroll> elastic_overscroll)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      hud_layer_(0),
      currently_scrolling_layer_(NULL),
      root_layer_scroll_offset_delegate_(NULL),
      background_color_(0),
      has_transparent_background_(false),
      overscroll_elasticity_layer_(NULL),
      page_scale_layer_(NULL),
      inner_viewport_scroll_layer_(NULL),
      outer_viewport_scroll_layer_(NULL),
      page_scale_factor_(page_scale_factor),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      elastic_overscroll_(elastic_overscroll),
      scrolling_layer_id_from_previous_tree_(0),
      contents_textures_purged_(false),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true),
      next_activation_forces_redraw_(false),
      has_ever_been_drawn_(false),
      render_surface_layer_list_id_(0),
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0),
      top_controls_shown_ratio_(top_controls_shown_ratio) {
}

LayerTreeImpl::~LayerTreeImpl() {
  BreakSwapPromises(SwapPromise::SWAP_FAILS);

  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  DCHECK(!root_layer_);
  DCHECK(layers_with_copy_output_request_.empty());
}

void LayerTreeImpl::Shutdown() {
  root_layer_ = nullptr;
}

void LayerTreeImpl::ReleaseResources() {
  if (root_layer_) {
    LayerTreeHostCommon::CallFunctionForSubtree(
        root_layer_.get(), [](LayerImpl* layer) { layer->ReleaseResources(); });
  }
}

void LayerTreeImpl::RecreateResources() {
  if (root_layer_) {
    LayerTreeHostCommon::CallFunctionForSubtree(
        root_layer_.get(),
        [](LayerImpl* layer) { layer->RecreateResources(); });
  }
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  if (inner_viewport_scroll_layer_)
    inner_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  inner_viewport_scroll_delegate_proxy_ = nullptr;
  outer_viewport_scroll_delegate_proxy_ = nullptr;

  root_layer_ = layer.Pass();
  currently_scrolling_layer_ = NULL;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
  page_scale_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

LayerImpl* LayerTreeImpl::InnerViewportScrollLayer() const {
  return inner_viewport_scroll_layer_;
}

LayerImpl* LayerTreeImpl::OuterViewportScrollLayer() const {
  return outer_viewport_scroll_layer_;
}

gfx::ScrollOffset LayerTreeImpl::TotalScrollOffset() const {
  gfx::ScrollOffset offset;

  if (inner_viewport_scroll_layer_)
    offset += inner_viewport_scroll_layer_->CurrentScrollOffset();

  if (outer_viewport_scroll_layer_)
    offset += outer_viewport_scroll_layer_->CurrentScrollOffset();

  return offset;
}

gfx::ScrollOffset LayerTreeImpl::TotalMaxScrollOffset() const {
  gfx::ScrollOffset offset;

  if (inner_viewport_scroll_layer_)
    offset += inner_viewport_scroll_layer_->MaxScrollOffset();

  if (outer_viewport_scroll_layer_)
    offset += outer_viewport_scroll_layer_->MaxScrollOffset();

  return offset;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  if (inner_viewport_scroll_layer_)
    inner_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_layer_->SetScrollOffsetDelegate(NULL);
  inner_viewport_scroll_delegate_proxy_ = nullptr;
  outer_viewport_scroll_delegate_proxy_ = nullptr;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
  page_scale_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  if (next_activation_forces_redraw_) {
    target_tree->ForceRedrawNextActivation();
    next_activation_forces_redraw_ = false;
  }

  target_tree->PassSwapPromises(&swap_promise_list_);

  target_tree->set_top_controls_shrink_blink_size(
      top_controls_shrink_blink_size_);
  target_tree->set_top_controls_height(top_controls_height_);
  target_tree->PushTopControls(nullptr);

  // Active tree already shares the page_scale_factor object with pending
  // tree so only the limits need to be provided.
  target_tree->PushPageScaleFactorAndLimits(nullptr, min_page_scale_factor(),
                                            max_page_scale_factor());
  target_tree->elastic_overscroll()->PushPendingToActive();

  target_tree->pending_page_scale_animation_ =
      pending_page_scale_animation_.Pass();

  if (page_scale_layer_ && inner_viewport_scroll_layer_) {
    target_tree->SetViewportLayersFromIds(
        overscroll_elasticity_layer_ ? overscroll_elasticity_layer_->id()
                                     : Layer::INVALID_ID,
        page_scale_layer_->id(), inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                     : Layer::INVALID_ID);
  } else {
    target_tree->ClearViewportLayers();
  }

  target_tree->RegisterSelection(selection_start_, selection_end_);

  // This should match the property synchronization in
  // LayerTreeHost::finishCommitOnImplThread().
  target_tree->set_source_frame_number(source_frame_number());
  target_tree->set_background_color(background_color());
  target_tree->set_has_transparent_background(has_transparent_background());

  if (ContentsTexturesPurged())
    target_tree->SetContentsTexturesPurged();
  else
    target_tree->ResetContentsTexturesPurged();

  if (ViewportSizeInvalid())
    target_tree->SetViewportSizeInvalid();
  else
    target_tree->ResetViewportSizeInvalid();

  if (hud_layer())
    target_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(
        LayerTreeHostCommon::FindLayerInSubtree(
            target_tree->root_layer(), hud_layer()->id())));
  else
    target_tree->set_hud_layer(NULL);

  target_tree->has_ever_been_drawn_ = false;
}

LayerImpl* LayerTreeImpl::InnerViewportContainerLayer() const {
  return inner_viewport_scroll_layer_
             ? inner_viewport_scroll_layer_->scroll_clip_layer()
             : NULL;
}

LayerImpl* LayerTreeImpl::OuterViewportContainerLayer() const {
  return outer_viewport_scroll_layer_
             ? outer_viewport_scroll_layer_->scroll_clip_layer()
             : NULL;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() const {
  DCHECK(IsActiveTree());
  return currently_scrolling_layer_;
}

void LayerTreeImpl::SetCurrentlyScrollingLayer(LayerImpl* layer) {
  if (currently_scrolling_layer_ == layer)
    return;

  if (currently_scrolling_layer_ &&
      currently_scrolling_layer_->scrollbar_animation_controller())
    currently_scrolling_layer_->scrollbar_animation_controller()
        ->DidScrollEnd();
  currently_scrolling_layer_ = layer;
  if (layer && layer->scrollbar_animation_controller())
    layer->scrollbar_animation_controller()->DidScrollBegin();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  SetCurrentlyScrollingLayer(NULL);
  scrolling_layer_id_from_previous_tree_ = 0;
}

namespace {

void ForceScrollbarParameterUpdateAfterScaleChange(LayerImpl* current_layer) {
  if (!current_layer)
    return;

  while (current_layer) {
    current_layer->ScrollbarParametersDidChange(false);
    current_layer = current_layer->parent();
  }
}

}  // namespace

float LayerTreeImpl::ClampPageScaleFactorToLimits(
    float page_scale_factor) const {
  if (min_page_scale_factor_ && page_scale_factor < min_page_scale_factor_)
    page_scale_factor = min_page_scale_factor_;
  else if (max_page_scale_factor_ && page_scale_factor > max_page_scale_factor_)
    page_scale_factor = max_page_scale_factor_;
  return page_scale_factor;
}

void LayerTreeImpl::SetPageScaleOnActiveTree(float active_page_scale) {
  DCHECK(IsActiveTree());
  if (page_scale_factor()->SetCurrent(
          ClampPageScaleFactorToLimits(active_page_scale)))
    DidUpdatePageScale();
}

void LayerTreeImpl::PushPageScaleFromMainThread(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  PushPageScaleFactorAndLimits(&page_scale_factor, min_page_scale_factor,
                               max_page_scale_factor);
}

void LayerTreeImpl::PushPageScaleFactorAndLimits(const float* page_scale_factor,
                                                 float min_page_scale_factor,
                                                 float max_page_scale_factor) {
  DCHECK(page_scale_factor || IsActiveTree());
  bool changed_page_scale = false;
  if (page_scale_factor) {
    DCHECK(!IsActiveTree() || !layer_tree_host_impl_->pending_tree());
    changed_page_scale |=
        page_scale_factor_->PushFromMainThread(*page_scale_factor);
  }
  if (IsActiveTree())
    changed_page_scale |= page_scale_factor_->PushPendingToActive();
  changed_page_scale |=
      SetPageScaleFactorLimits(min_page_scale_factor, max_page_scale_factor);

  if (changed_page_scale)
    DidUpdatePageScale();
}

void LayerTreeImpl::set_top_controls_shrink_blink_size(bool shrink) {
  if (top_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_shrink_blink_size_ = shrink;
  if (IsActiveTree())
    layer_tree_host_impl_->UpdateViewportContainerSizes();
}

void LayerTreeImpl::set_top_controls_height(float top_controls_height) {
  if (top_controls_height_ == top_controls_height)
    return;

  top_controls_height_ = top_controls_height;
  if (IsActiveTree())
    layer_tree_host_impl_->UpdateViewportContainerSizes();
}

bool LayerTreeImpl::SetCurrentTopControlsShownRatio(float ratio) {
  ratio = std::max(ratio, 0.f);
  ratio = std::min(ratio, 1.f);
  return top_controls_shown_ratio_->SetCurrent(ratio);
}

void LayerTreeImpl::PushTopControlsFromMainThread(
    float top_controls_shown_ratio) {
  PushTopControls(&top_controls_shown_ratio);
}

void LayerTreeImpl::PushTopControls(const float* top_controls_shown_ratio) {
  DCHECK(top_controls_shown_ratio || IsActiveTree());

  if (top_controls_shown_ratio) {
    DCHECK(!IsActiveTree() || !layer_tree_host_impl_->pending_tree());
    top_controls_shown_ratio_->PushFromMainThread(*top_controls_shown_ratio);
  }
  if (IsActiveTree()) {
    if (top_controls_shown_ratio_->PushPendingToActive())
      layer_tree_host_impl_->DidChangeTopControlsPosition();
  }
}

bool LayerTreeImpl::SetPageScaleFactorLimits(float min_page_scale_factor,
                                             float max_page_scale_factor) {
  if (min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return false;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;

  return true;
}

void LayerTreeImpl::DidUpdatePageScale() {
  if (IsActiveTree())
    page_scale_factor()->SetCurrent(
        ClampPageScaleFactorToLimits(current_page_scale_factor()));

  set_needs_update_draw_properties();

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->UpdateRootLayerState(
        TotalScrollOffset(), TotalMaxScrollOffset(), ScrollableSize(),
        current_page_scale_factor(), min_page_scale_factor_,
        max_page_scale_factor_);
  }

  ForceScrollbarParameterUpdateAfterScaleChange(page_scale_layer());

  HideInnerViewportScrollbarsIfNearMinimumScale();
}

void LayerTreeImpl::HideInnerViewportScrollbarsIfNearMinimumScale() {
  if (!InnerViewportContainerLayer())
    return;

  LayerImpl::ScrollbarSet* scrollbars =
      InnerViewportContainerLayer()->scrollbars();

  if (!scrollbars)
    return;

  for (LayerImpl::ScrollbarSet::iterator it = scrollbars->begin();
       it != scrollbars->end();
       ++it) {
    ScrollbarLayerImplBase* scrollbar = *it;
    float minimum_scale_to_show_at =
        min_page_scale_factor() * settings().scrollbar_show_scale_threshold;
    scrollbar->SetHideLayerAndSubtree(
        current_page_scale_factor() < minimum_scale_to_show_at);
  }
}

SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() {
  return page_scale_factor_.get();
}

const SyncedProperty<ScaleGroup>* LayerTreeImpl::page_scale_factor() const {
  return page_scale_factor_.get();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  if (!InnerViewportContainerLayer())
    return gfx::SizeF();

  return gfx::ScaleSize(InnerViewportContainerLayer()->BoundsForScrolling(),
                        1.0f / current_page_scale_factor());
}

gfx::Rect LayerTreeImpl::RootScrollLayerDeviceViewportBounds() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer || root_scroll_layer->children().empty())
    return gfx::Rect();
  LayerImpl* layer = root_scroll_layer->children()[0];
  return MathUtil::MapEnclosingClippedRect(layer->screen_space_transform(),
                                           gfx::Rect(layer->content_bounds()));
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor()->AbortCommit();
  top_controls_shown_ratio()->AbortCommit();
  elastic_overscroll()->AbortCommit();

  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), [](LayerImpl* layer) {
        layer->ApplySentScrollDeltasFromAbortedCommit();
      });
}

void LayerTreeImpl::SetViewportLayersFromIds(
    int overscroll_elasticity_layer_id,
    int page_scale_layer_id,
    int inner_viewport_scroll_layer_id,
    int outer_viewport_scroll_layer_id) {
  overscroll_elasticity_layer_ = LayerById(overscroll_elasticity_layer_id);
  page_scale_layer_ = LayerById(page_scale_layer_id);
  DCHECK(page_scale_layer_);

  inner_viewport_scroll_layer_ =
      LayerById(inner_viewport_scroll_layer_id);
  DCHECK(inner_viewport_scroll_layer_);

  outer_viewport_scroll_layer_ =
      LayerById(outer_viewport_scroll_layer_id);
  DCHECK(outer_viewport_scroll_layer_ ||
         outer_viewport_scroll_layer_id == Layer::INVALID_ID);

  HideInnerViewportScrollbarsIfNearMinimumScale();

  if (!root_layer_scroll_offset_delegate_)
    return;

  inner_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
      new LayerScrollOffsetDelegateProxy(inner_viewport_scroll_layer_,
                                         root_layer_scroll_offset_delegate_,
                                         this));

  if (outer_viewport_scroll_layer_)
    outer_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
        new LayerScrollOffsetDelegateProxy(outer_viewport_scroll_layer_,
                                           root_layer_scroll_offset_delegate_,
                                           this));
}

void LayerTreeImpl::ClearViewportLayers() {
  page_scale_layer_ = NULL;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
}

bool LayerTreeImpl::UpdateDrawProperties(bool update_lcd_text) {
  if (!needs_update_draw_properties_)
    return true;

  // Calling UpdateDrawProperties must clear this flag, so there can be no
  // early outs before this.
  needs_update_draw_properties_ = false;

  // For max_texture_size.  When the renderer is re-created in
  // CreateAndSetRenderer, the needs update draw properties flag is set
  // again.
  if (!layer_tree_host_impl_->renderer())
    return false;

  // Clear this after the renderer early out, as it should still be
  // possible to hit test even without a renderer.
  render_surface_layer_list_.clear();

  if (!root_layer())
    return false;

  {
    TRACE_EVENT2(
        "cc", "LayerTreeImpl::UpdateDrawProperties::CalculateDrawProperties",
        "IsActive", IsActiveTree(), "SourceFrameNumber", source_frame_number_);
    LayerImpl* page_scale_layer =
        page_scale_layer_ ? page_scale_layer_ : InnerViewportContainerLayer();
    bool can_render_to_separate_surface =
        (layer_tree_host_impl_->GetDrawMode() !=
         DRAW_MODE_RESOURCELESS_SOFTWARE);

    ++render_surface_layer_list_id_;
    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        root_layer(), DrawViewportSize(),
        layer_tree_host_impl_->DrawTransform(), device_scale_factor(),
        current_page_scale_factor(), page_scale_layer,
        elastic_overscroll()->Current(IsActiveTree()),
        overscroll_elasticity_layer_, resource_provider()->max_texture_size(),
        settings().can_use_lcd_text, settings().layers_always_allowed_lcd_text,
        can_render_to_separate_surface,
        settings().layer_transforms_should_scale_layer_contents,
        settings().verify_property_trees,
        &render_surface_layer_list_, render_surface_layer_list_id_);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  {
    TRACE_EVENT2("cc", "LayerTreeImpl::UpdateDrawProperties::Occlusion",
                 "IsActive", IsActiveTree(), "SourceFrameNumber",
                 source_frame_number_);
    OcclusionTracker<LayerImpl> occlusion_tracker(
        root_layer()->render_surface()->content_rect());
    occlusion_tracker.set_minimum_tracking_size(
        settings().minimum_occlusion_tracking_size);

    // LayerIterator is used here instead of CallFunctionForSubtree to only
    // UpdateTilePriorities on layers that will be visible (and thus have valid
    // draw properties) and not because any ordering is required.
    auto end = LayerIterator<LayerImpl>::End(&render_surface_layer_list_);
    for (auto it = LayerIterator<LayerImpl>::Begin(&render_surface_layer_list_);
         it != end; ++it) {
      occlusion_tracker.EnterLayer(it);

      // There are very few render targets so this should be cheap to do for
      // each layer instead of something more complicated.
      bool inside_replica = false;
      LayerImpl* layer = it->render_target();
      while (layer && !inside_replica) {
        if (layer->render_target()->has_replica())
          inside_replica = true;
        layer = layer->render_target()->parent();
      }

      // Don't use occlusion if a layer will appear in a replica, since the
      // tile raster code does not know how to look for the replica and would
      // consider it occluded even though the replica is visible.
      // Since occlusion is only used for browser compositor (i.e.
      // use_occlusion_for_tile_prioritization) and it won't use replicas,
      // this should matter not.

      if (it.represents_itself()) {
        Occlusion occlusion =
            inside_replica ? Occlusion()
                           : occlusion_tracker.GetCurrentOcclusionForLayer(
                                 it->draw_transform());
        it->draw_properties().occlusion_in_content_space = occlusion;
      }

      if (it.represents_contributing_render_surface()) {
        // Surfaces aren't used by the tile raster code, so they can have
        // occlusion regardless of replicas.
        Occlusion occlusion =
            occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                it->render_surface()->draw_transform());
        it->render_surface()->set_occlusion_in_content_space(occlusion);
        // Masks are used to draw the contributing surface, so should have
        // the same occlusion as the surface (nothing inside the surface
        // occludes them).
        if (LayerImpl* mask = it->mask_layer()) {
          Occlusion mask_occlusion =
              inside_replica
                  ? Occlusion()
                  : occlusion_tracker.GetCurrentOcclusionForContributingSurface(
                        it->render_surface()->draw_transform() *
                        it->draw_transform());
          mask->draw_properties().occlusion_in_content_space = mask_occlusion;
        }
        if (LayerImpl* replica = it->replica_layer()) {
          if (LayerImpl* mask = replica->mask_layer())
            mask->draw_properties().occlusion_in_content_space = Occlusion();
        }
      }

      occlusion_tracker.LeaveLayer(it);
    }

    unoccluded_screen_space_region_ =
        occlusion_tracker.ComputeVisibleRegionInScreen();
  }

  // It'd be ideal if this could be done earlier, but when the raster source
  // is updated from the main thread during push properties, update draw
  // properties has not occurred yet and so it's not clear whether or not the
  // layer can or cannot use lcd text.  So, this is the cleanup pass to
  // determine if the raster source needs to be replaced with a non-lcd
  // raster source due to draw properties.
  if (update_lcd_text) {
    // TODO(enne): Make LTHI::sync_tree return this value.
    LayerTreeImpl* sync_tree =
        layer_tree_host_impl_->proxy()->CommitToActiveTree()
            ? layer_tree_host_impl_->active_tree()
            : layer_tree_host_impl_->pending_tree();
    // If this is not the sync tree, then it is not safe to update lcd text
    // as it causes invalidations and the tiles may be in use.
    DCHECK_EQ(this, sync_tree);
    for (const auto& layer : picture_layers_)
      layer->UpdateCanUseLCDTextAfterCommit();
  }

  {
    TRACE_EVENT_BEGIN2("cc", "LayerTreeImpl::UpdateDrawProperties::UpdateTiles",
                       "IsActive", IsActiveTree(), "SourceFrameNumber",
                       source_frame_number_);
    const bool resourceless_software_draw =
        (layer_tree_host_impl_->GetDrawMode() ==
         DRAW_MODE_RESOURCELESS_SOFTWARE);
    size_t layers_updated_count = 0;
    bool tile_priorities_updated = false;
    for (PictureLayerImpl* layer : picture_layers_) {
      if (!layer->IsDrawnRenderSurfaceLayerListMember())
        continue;
      ++layers_updated_count;
      tile_priorities_updated |= layer->UpdateTiles(resourceless_software_draw);
    }

    if (tile_priorities_updated)
      DidModifyTilePriorities();

    TRACE_EVENT_END1("cc", "LayerTreeImpl::UpdateTilePriorities",
                     "layers_updated_count", layers_updated_count);
  }

  DCHECK(!needs_update_draw_properties_) <<
      "CalcDrawProperties should not set_needs_update_draw_properties()";
  return true;
}

const LayerImplList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

const Region& LayerTreeImpl::UnoccludedScreenSpaceRegion() const {
  // If this assert triggers, then the render_surface_layer_list_ is dirty, so
  // the unoccluded_screen_space_region_ is not valid anymore.
  DCHECK(!needs_update_draw_properties_);
  return unoccluded_screen_space_region_;
}

gfx::Size LayerTreeImpl::ScrollableSize() const {
  LayerImpl* root_scroll_layer = OuterViewportScrollLayer()
                                     ? OuterViewportScrollLayer()
                                     : InnerViewportScrollLayer();
  if (!root_scroll_layer || root_scroll_layer->children().empty())
    return gfx::Size();
  return root_scroll_layer->children()[0]->bounds();
}

LayerImpl* LayerTreeImpl::LayerById(int id) {
  LayerIdMap::iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : NULL;
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layer_id_map_.erase(layer->id());
}

size_t LayerTreeImpl::NumLayers() {
  return layer_id_map_.size();
}

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pending_tree) {
  pending_tree->SetCurrentlyScrollingLayer(
      LayerTreeHostCommon::FindLayerInSubtree(pending_tree->root_layer(),
          currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0));
}

void LayerTreeImpl::DidBecomeActive() {
  if (next_activation_forces_redraw_) {
    layer_tree_host_impl_->SetFullRootLayerDamage();
    next_activation_forces_redraw_ = false;
  }

  if (scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::FindLayerInSubtree(
        root_layer(), scrolling_layer_id_from_previous_tree_);
  }

  // Always reset this flag on activation, as we would only have activated
  // if we were in a good state.
  layer_tree_host_impl_->ResetRequiresHighResToDraw();

  if (root_layer()) {
    LayerTreeHostCommon::CallFunctionForSubtree(
        root_layer(), [](LayerImpl* layer) { layer->DidBecomeActive(); });
  }

  devtools_instrumentation::DidActivateLayerTree(layer_tree_host_impl_->id(),
                                                 source_frame_number_);
}

bool LayerTreeImpl::ContentsTexturesPurged() const {
  return contents_textures_purged_;
}

void LayerTreeImpl::SetContentsTexturesPurged() {
  if (contents_textures_purged_)
    return;
  contents_textures_purged_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetContentsTexturesPurged() {
  if (!contents_textures_purged_)
    return;
  contents_textures_purged_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

bool LayerTreeImpl::RequiresHighResToDraw() const {
  return layer_tree_host_impl_->RequiresHighResToDraw();
}

bool LayerTreeImpl::ViewportSizeInvalid() const {
  return viewport_size_invalid_;
}

void LayerTreeImpl::SetViewportSizeInvalid() {
  viewport_size_invalid_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::ResetViewportSizeInvalid() {
  viewport_size_invalid_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

Proxy* LayerTreeImpl::proxy() const {
  return layer_tree_host_impl_->proxy();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debug_state();
}

const RendererCapabilitiesImpl& LayerTreeImpl::GetRendererCapabilities() const {
  return layer_tree_host_impl_->GetRendererCapabilities();
}

ContextProvider* LayerTreeImpl::context_provider() const {
  return output_surface()->context_provider();
}

OutputSurface* LayerTreeImpl::output_surface() const {
  return layer_tree_host_impl_->output_surface();
}

ResourceProvider* LayerTreeImpl::resource_provider() const {
  return layer_tree_host_impl_->resource_provider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return layer_tree_host_impl_->tile_manager();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fps_counter();
}

PaintTimeCounter* LayerTreeImpl::paint_time_counter() const {
  return layer_tree_host_impl_->paint_time_counter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return layer_tree_host_impl_->memory_history();
}

gfx::Size LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->device_viewport_size();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->device_scale_factor();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debug_rect_history();
}

bool LayerTreeImpl::IsActiveTree() const {
  return layer_tree_host_impl_->active_tree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return layer_tree_host_impl_->pending_tree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return layer_tree_host_impl_->recycle_tree() == this;
}

bool LayerTreeImpl::IsSyncTree() const {
  return layer_tree_host_impl_->sync_tree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->active_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->pending_tree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinch_gesture_active();
}

BeginFrameArgs LayerTreeImpl::CurrentBeginFrameArgs() const {
  return layer_tree_host_impl_->CurrentBeginFrameArgs();
}

base::TimeDelta LayerTreeImpl::begin_impl_frame_interval() const {
  return layer_tree_host_impl_->begin_impl_frame_interval();
}

void LayerTreeImpl::SetNeedsCommit() {
  layer_tree_host_impl_->SetNeedsCommit();
}

gfx::Rect LayerTreeImpl::DeviceViewport() const {
  return layer_tree_host_impl_->DeviceViewport();
}

gfx::Size LayerTreeImpl::DrawViewportSize() const {
  return layer_tree_host_impl_->DrawViewportSize();
}

const gfx::Rect LayerTreeImpl::ViewportRectForTilePriority() const {
  return layer_tree_host_impl_->ViewportRectForTilePriority();
}

scoped_ptr<ScrollbarAnimationController>
LayerTreeImpl::CreateScrollbarAnimationController(LayerImpl* scrolling_layer) {
  DCHECK(settings().scrollbar_fade_delay_ms);
  DCHECK(settings().scrollbar_fade_duration_ms);
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_delay_ms);
  base::TimeDelta resize_delay = base::TimeDelta::FromMilliseconds(
      settings().scrollbar_fade_resize_delay_ms);
  base::TimeDelta duration =
      base::TimeDelta::FromMilliseconds(settings().scrollbar_fade_duration_ms);
  switch (settings().scrollbar_animator) {
    case LayerTreeSettings::LINEAR_FADE: {
      return ScrollbarAnimationControllerLinearFade::Create(
          scrolling_layer,
          layer_tree_host_impl_,
          delay,
          resize_delay,
          duration);
    }
    case LayerTreeSettings::THINNING: {
      return ScrollbarAnimationControllerThinning::Create(scrolling_layer,
                                                          layer_tree_host_impl_,
                                                          delay,
                                                          resize_delay,
                                                          duration);
    }
    case LayerTreeSettings::NO_ANIMATOR:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void LayerTreeImpl::DidAnimateScrollOffset() {
  layer_tree_host_impl_->DidAnimateScrollOffset();
}

bool LayerTreeImpl::use_gpu_rasterization() const {
  return layer_tree_host_impl_->use_gpu_rasterization();
}

GpuRasterizationStatus LayerTreeImpl::GetGpuRasterizationStatus() const {
  return layer_tree_host_impl_->gpu_rasterization_status();
}

bool LayerTreeImpl::create_low_res_tiling() const {
  return layer_tree_host_impl_->create_low_res_tiling();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->SetNeedsRedraw();
}

AnimationRegistrar* LayerTreeImpl::GetAnimationRegistrar() const {
  return layer_tree_host_impl_->animation_registrar();
}

void LayerTreeImpl::GetAllTilesForTracing(std::set<const Tile*>* tiles) const {
  typedef LayerIterator<LayerImpl> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(&render_surface_layer_list_);
       it != end;
       ++it) {
    if (!it.represents_itself())
      continue;
    LayerImpl* layer_impl = *it;
    layer_impl->GetAllTilesForTracing(tiles);
  }
}

void LayerTreeImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  TracedValue::MakeDictIntoImplicitSnapshot(state, "cc::LayerTreeImpl", this);
  state->SetInteger("source_frame_number", source_frame_number_);

  state->BeginDictionary("root_layer");
  root_layer_->AsValueInto(state);
  state->EndDictionary();

  state->BeginArray("render_surface_layer_list");
  typedef LayerIterator<LayerImpl> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::Begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (!it.represents_itself())
      continue;
    TracedValue::AppendIDRef(*it, state);
  }
  state->EndArray();

  state->BeginArray("swap_promise_trace_ids");
  for (size_t i = 0; i < swap_promise_list_.size(); i++)
    state->AppendDouble(swap_promise_list_[i]->TraceId());
  state->EndArray();
}

void LayerTreeImpl::SetRootLayerScrollOffsetDelegate(
    LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) {
  if (root_layer_scroll_offset_delegate_ == root_layer_scroll_offset_delegate)
    return;

  if (!root_layer_scroll_offset_delegate) {
    // Make sure we remove the proxies from their layers before
    // releasing them.
    if (InnerViewportScrollLayer())
      InnerViewportScrollLayer()->SetScrollOffsetDelegate(NULL);
    if (OuterViewportScrollLayer())
      OuterViewportScrollLayer()->SetScrollOffsetDelegate(NULL);
    inner_viewport_scroll_delegate_proxy_ = nullptr;
    outer_viewport_scroll_delegate_proxy_ = nullptr;
  }

  root_layer_scroll_offset_delegate_ = root_layer_scroll_offset_delegate;

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->UpdateRootLayerState(
        TotalScrollOffset(), TotalMaxScrollOffset(), ScrollableSize(),
        current_page_scale_factor(), min_page_scale_factor(),
        max_page_scale_factor());

    if (inner_viewport_scroll_layer_) {
      inner_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
          new LayerScrollOffsetDelegateProxy(InnerViewportScrollLayer(),
                                             root_layer_scroll_offset_delegate_,
                                             this));
      inner_viewport_scroll_layer_->SetScrollOffsetDelegate(
          inner_viewport_scroll_delegate_proxy_.get());
    }

    if (outer_viewport_scroll_layer_) {
      outer_viewport_scroll_delegate_proxy_ = make_scoped_ptr(
          new LayerScrollOffsetDelegateProxy(OuterViewportScrollLayer(),
                                             root_layer_scroll_offset_delegate_,
                                             this));
      outer_viewport_scroll_layer_->SetScrollOffsetDelegate(
          outer_viewport_scroll_delegate_proxy_.get());
    }

    if (inner_viewport_scroll_layer_)
      inner_viewport_scroll_layer_->RefreshFromScrollDelegate();
    if (outer_viewport_scroll_layer_)
      outer_viewport_scroll_layer_->RefreshFromScrollDelegate();

    if (inner_viewport_scroll_layer_)
      UpdateScrollOffsetDelegate();
  }
}

void LayerTreeImpl::OnRootLayerDelegatedScrollOffsetChanged() {
  DCHECK(root_layer_scroll_offset_delegate_);
  if (inner_viewport_scroll_layer_) {
    inner_viewport_scroll_layer_->RefreshFromScrollDelegate();
  }
  if (outer_viewport_scroll_layer_) {
    outer_viewport_scroll_layer_->RefreshFromScrollDelegate();
  }
}

void LayerTreeImpl::UpdateScrollOffsetDelegate() {
  DCHECK(InnerViewportScrollLayer());
  DCHECK(!OuterViewportScrollLayer() || outer_viewport_scroll_delegate_proxy_);
  DCHECK(root_layer_scroll_offset_delegate_);

  gfx::ScrollOffset offset =
      inner_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  if (OuterViewportScrollLayer())
    offset += outer_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  root_layer_scroll_offset_delegate_->UpdateRootLayerState(
      offset, TotalMaxScrollOffset(), ScrollableSize(),
      current_page_scale_factor(), min_page_scale_factor(),
      max_page_scale_factor());
}

gfx::ScrollOffset LayerTreeImpl::GetDelegatedScrollOffset(LayerImpl* layer) {
  DCHECK(root_layer_scroll_offset_delegate_);
  DCHECK(InnerViewportScrollLayer());
  if (layer == InnerViewportScrollLayer() && !OuterViewportScrollLayer())
    return root_layer_scroll_offset_delegate_->GetTotalScrollOffset();

  // If we get here, we have both inner/outer viewports, and need to distribute
  // the scroll offset between them.
  DCHECK(inner_viewport_scroll_delegate_proxy_);
  DCHECK(outer_viewport_scroll_delegate_proxy_);
  gfx::ScrollOffset inner_viewport_offset =
      inner_viewport_scroll_delegate_proxy_->last_set_scroll_offset();
  gfx::ScrollOffset outer_viewport_offset =
      outer_viewport_scroll_delegate_proxy_->last_set_scroll_offset();

  // It may be nothing has changed.
  gfx::ScrollOffset delegate_offset =
      root_layer_scroll_offset_delegate_->GetTotalScrollOffset();
  if (inner_viewport_offset + outer_viewport_offset == delegate_offset) {
    if (layer == InnerViewportScrollLayer())
      return inner_viewport_offset;
    else
      return outer_viewport_offset;
  }

  gfx::ScrollOffset max_outer_viewport_scroll_offset =
      OuterViewportScrollLayer()->MaxScrollOffset();

  outer_viewport_offset = delegate_offset - inner_viewport_offset;
  outer_viewport_offset.SetToMin(max_outer_viewport_scroll_offset);
  outer_viewport_offset.SetToMax(gfx::ScrollOffset());

  if (layer == OuterViewportScrollLayer())
    return outer_viewport_offset;

  inner_viewport_offset = delegate_offset - outer_viewport_offset;

  return inner_viewport_offset;
}

void LayerTreeImpl::QueueSwapPromise(scoped_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(swap_promise.Pass());
}

void LayerTreeImpl::PassSwapPromises(
    ScopedPtrVector<SwapPromise>* new_swap_promise) {
  swap_promise_list_.insert_and_take(swap_promise_list_.end(),
                                     new_swap_promise);
  new_swap_promise->clear();
}

void LayerTreeImpl::FinishSwapPromises(CompositorFrameMetadata* metadata) {
  for (size_t i = 0; i < swap_promise_list_.size(); i++)
    swap_promise_list_[i]->DidSwap(metadata);
  swap_promise_list_.clear();
}

void LayerTreeImpl::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (size_t i = 0; i < swap_promise_list_.size(); i++)
    swap_promise_list_[i]->DidNotSwap(reason);
  swap_promise_list_.clear();
}

void LayerTreeImpl::DidModifyTilePriorities() {
  layer_tree_host_impl_->DidModifyTilePriorities();
}

void LayerTreeImpl::set_ui_resource_request_queue(
    const UIResourceRequestQueue& queue) {
  ui_resource_request_queue_ = queue;
}

ResourceProvider::ResourceId LayerTreeImpl::ResourceIdForUIResource(
    UIResourceId uid) const {
  return layer_tree_host_impl_->ResourceIdForUIResource(uid);
}

bool LayerTreeImpl::IsUIResourceOpaque(UIResourceId uid) const {
  return layer_tree_host_impl_->IsUIResourceOpaque(uid);
}

void LayerTreeImpl::ProcessUIResourceRequestQueue() {
  for (const auto& req : ui_resource_request_queue_) {
    switch (req.GetType()) {
      case UIResourceRequest::UI_RESOURCE_CREATE:
        layer_tree_host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UI_RESOURCE_DELETE:
        layer_tree_host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UI_RESOURCE_INVALID_REQUEST:
        NOTREACHED();
        break;
    }
  }
  ui_resource_request_queue_.clear();

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (layer_tree_host_impl_->EvictedUIResourcesExist())
    layer_tree_host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::RegisterPictureLayerImpl(PictureLayerImpl* layer) {
  DCHECK(std::find(picture_layers_.begin(), picture_layers_.end(), layer) ==
         picture_layers_.end());
  picture_layers_.push_back(layer);
}

void LayerTreeImpl::UnregisterPictureLayerImpl(PictureLayerImpl* layer) {
  std::vector<PictureLayerImpl*>::iterator it =
      std::find(picture_layers_.begin(), picture_layers_.end(), layer);
  DCHECK(it != picture_layers_.end());
  picture_layers_.erase(it);
}

void LayerTreeImpl::AddLayerWithCopyOutputRequest(LayerImpl* layer) {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  // DCHECK(std::find(layers_with_copy_output_request_.begin(),
  //                 layers_with_copy_output_request_.end(),
  //                 layer) == layers_with_copy_output_request_.end());
  // TODO(danakj): Remove this once crash is found crbug.com/309777
  for (size_t i = 0; i < layers_with_copy_output_request_.size(); ++i) {
    CHECK(layers_with_copy_output_request_[i] != layer)
        << i << " of " << layers_with_copy_output_request_.size();
  }
  layers_with_copy_output_request_.push_back(layer);
}

void LayerTreeImpl::RemoveLayerWithCopyOutputRequest(LayerImpl* layer) {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  std::vector<LayerImpl*>::iterator it = std::find(
      layers_with_copy_output_request_.begin(),
      layers_with_copy_output_request_.end(),
      layer);
  DCHECK(it != layers_with_copy_output_request_.end());
  layers_with_copy_output_request_.erase(it);

  // TODO(danakj): Remove this once crash is found crbug.com/309777
  for (size_t i = 0; i < layers_with_copy_output_request_.size(); ++i) {
    CHECK(layers_with_copy_output_request_[i] != layer)
        << i << " of " << layers_with_copy_output_request_.size();
  }
}

const std::vector<LayerImpl*>& LayerTreeImpl::LayersWithCopyOutputRequest()
    const {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  return layers_with_copy_output_request_;
}

template <typename LayerType>
static inline bool LayerClipsSubtree(LayerType* layer) {
  return layer->masks_to_bounds() || layer->mask_layer();
}

static bool PointHitsRect(
    const gfx::PointF& screen_space_point,
    const gfx::Transform& local_space_to_screen_space_transform,
    const gfx::RectF& local_space_rect,
    float* distance_to_camera) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this rect.
  gfx::Transform inverse_local_space_to_screen_space(
      gfx::Transform::kSkipInitialization);
  if (!local_space_to_screen_space_transform.GetInverse(
          &inverse_local_space_to_screen_space))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given rect.
  bool clipped = false;
  gfx::Point3F planar_point = MathUtil::ProjectPoint3D(
      inverse_local_space_to_screen_space, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_local_space =
      gfx::PointF(planar_point.x(), planar_point.y());

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this rect.
  if (clipped)
    return false;

  if (!local_space_rect.Contains(hit_test_point_in_local_space))
    return false;

  if (distance_to_camera) {
    // To compute the distance to the camera, we have to take the planar point
    // and pull it back to world space and compute the displacement along the
    // z-axis.
    gfx::Point3F planar_point_in_screen_space(planar_point);
    local_space_to_screen_space_transform.TransformPoint(
        &planar_point_in_screen_space);
    *distance_to_camera = planar_point_in_screen_space.z();
  }

  return true;
}

static bool PointHitsRegion(const gfx::PointF& screen_space_point,
                            const gfx::Transform& screen_space_transform,
                            const Region& layer_space_region,
                            float layer_content_scale_x,
                            float layer_content_scale_y) {
  // If the transform is not invertible, then assume that this point doesn't hit
  // this region.
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  if (!screen_space_transform.GetInverse(&inverse_screen_space_transform))
    return false;

  // Transform the hit test point from screen space to the local space of the
  // given region.
  bool clipped = false;
  gfx::PointF hit_test_point_in_content_space = MathUtil::ProjectPoint(
      inverse_screen_space_transform, screen_space_point, &clipped);
  gfx::PointF hit_test_point_in_layer_space =
      gfx::ScalePoint(hit_test_point_in_content_space,
                      1.f / layer_content_scale_x,
                      1.f / layer_content_scale_y);

  // If ProjectPoint could not project to a valid value, then we assume that
  // this point doesn't hit this region.
  if (clipped)
    return false;

  return layer_space_region.Contains(
      gfx::ToRoundedPoint(hit_test_point_in_layer_space));
}

static const LayerImpl* GetNextClippingLayer(const LayerImpl* layer) {
  if (layer->scroll_parent())
    return layer->scroll_parent();
  if (layer->clip_parent())
    return layer->clip_parent();
  return layer->parent();
}

static bool PointIsClippedBySurfaceOrClipRect(
    const gfx::PointF& screen_space_point,
    const LayerImpl* layer) {
  // Walk up the layer tree and hit-test any render_surfaces and any layer
  // clip rects that are active.
  for (; layer; layer = GetNextClippingLayer(layer)) {
    if (layer->render_surface() &&
        !PointHitsRect(screen_space_point,
                       layer->render_surface()->screen_space_transform(),
                       layer->render_surface()->content_rect(),
                       NULL))
      return true;

    if (LayerClipsSubtree(layer) &&
        !PointHitsRect(screen_space_point,
                       layer->screen_space_transform(),
                       gfx::Rect(layer->content_bounds()),
                       NULL))
      return true;
  }

  // If we have finished walking all ancestors without having already exited,
  // then the point is not clipped by any ancestors.
  return false;
}

static bool PointHitsLayer(const LayerImpl* layer,
                           const gfx::PointF& screen_space_point,
                           float* distance_to_intersection) {
  gfx::RectF content_rect(layer->content_bounds());
  if (!PointHitsRect(screen_space_point,
                     layer->screen_space_transform(),
                     content_rect,
                     distance_to_intersection))
    return false;

  // At this point, we think the point does hit the layer, but we need to walk
  // up the parents to ensure that the layer was not clipped in such a way
  // that the hit point actually should not hit the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer))
    return false;

  // Skip the HUD layer.
  if (layer == layer->layer_tree_impl()->hud_layer())
    return false;

  return true;
}

struct FindClosestMatchingLayerDataForRecursion {
  FindClosestMatchingLayerDataForRecursion()
      : closest_match(NULL),
        closest_distance(-std::numeric_limits<float>::infinity()) {}
  LayerImpl* closest_match;
  // Note that the positive z-axis points towards the camera, so bigger means
  // closer in this case, counterintuitively.
  float closest_distance;
};

template <typename Functor>
static void FindClosestMatchingLayer(
    const gfx::PointF& screen_space_point,
    LayerImpl* layer,
    const Functor& func,
    FindClosestMatchingLayerDataForRecursion* data_for_recursion) {
  for (int i = layer->children().size() - 1; i >= 0; --i) {
    FindClosestMatchingLayer(
        screen_space_point, layer->children()[i], func, data_for_recursion);
  }

  float distance_to_intersection = 0.f;
  if (func(layer) &&
      PointHitsLayer(layer, screen_space_point, &distance_to_intersection) &&
      ((!data_for_recursion->closest_match ||
        distance_to_intersection > data_for_recursion->closest_distance))) {
    data_for_recursion->closest_distance = distance_to_intersection;
    data_for_recursion->closest_match = layer;
  }
}

static bool ScrollsAnyDrawnRenderSurfaceLayerListMember(LayerImpl* layer) {
  if (!layer->scrollable())
    return false;
  if (layer->draw_properties().layer_or_descendant_is_drawn)
    return true;

  if (!layer->scroll_children())
    return false;
  for (std::set<LayerImpl*>::const_iterator it =
           layer->scroll_children()->begin();
       it != layer->scroll_children()->end();
       ++it) {
    if ((*it)->draw_properties().layer_or_descendant_is_drawn)
      return true;
  }
  return false;
}

struct FindScrollingLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return ScrollsAnyDrawnRenderSurfaceLayerListMember(layer);
  }
};

LayerImpl* LayerTreeImpl::FindFirstScrollingLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(screen_space_point,
                           root_layer(),
                           FindScrollingLayerFunctor(),
                           &data_for_recursion);
  return data_for_recursion.closest_match;
}

struct HitTestVisibleScrollableOrTouchableFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->IsDrawnRenderSurfaceLayerListMember() ||
           ScrollsAnyDrawnRenderSurfaceLayerListMember(layer) ||
           !layer->touch_event_handler_region().IsEmpty() ||
           layer->have_wheel_event_handlers();
  }
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (!root_layer())
    return NULL;
  bool update_lcd_text = false;
  if (!UpdateDrawProperties(update_lcd_text))
    return NULL;
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(screen_space_point,
                           root_layer(),
                           HitTestVisibleScrollableOrTouchableFunctor(),
                           &data_for_recursion);
  return data_for_recursion.closest_match;
}

static bool LayerHasTouchEventHandlersAt(const gfx::PointF& screen_space_point,
                                         LayerImpl* layer_impl) {
  if (layer_impl->touch_event_handler_region().IsEmpty())
    return false;

  if (!PointHitsRegion(screen_space_point,
                       layer_impl->screen_space_transform(),
                       layer_impl->touch_event_handler_region(),
                       layer_impl->contents_scale_x(),
                       layer_impl->contents_scale_y()))
    return false;

  // At this point, we think the point does hit the touch event handler region
  // on the layer, but we need to walk up the parents to ensure that the layer
  // was not clipped in such a way that the hit point actually should not hit
  // the layer.
  if (PointIsClippedBySurfaceOrClipRect(screen_space_point, layer_impl))
    return false;

  return true;
}

struct FindWheelEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return layer->have_wheel_event_handlers();
  }
};

LayerImpl* LayerTreeImpl::FindLayerWithWheelHandlerThatIsHitByPoint(
    const gfx::PointF& screen_space_point) {
  if (!root_layer())
    return NULL;
  bool update_lcd_text = false;
  if (!UpdateDrawProperties(update_lcd_text))
    return NULL;
  FindWheelEventLayerFunctor func;
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(screen_space_point, root_layer(), func,
                           &data_for_recursion);
  return data_for_recursion.closest_match;
}

struct FindTouchEventLayerFunctor {
  bool operator()(LayerImpl* layer) const {
    return LayerHasTouchEventHandlersAt(screen_space_point, layer);
  }
  const gfx::PointF screen_space_point;
};

LayerImpl* LayerTreeImpl::FindLayerThatIsHitByPointInTouchHandlerRegion(
    const gfx::PointF& screen_space_point) {
  if (!root_layer())
    return NULL;
  bool update_lcd_text = false;
  if (!UpdateDrawProperties(update_lcd_text))
    return NULL;
  FindTouchEventLayerFunctor func = {screen_space_point};
  FindClosestMatchingLayerDataForRecursion data_for_recursion;
  FindClosestMatchingLayer(
      screen_space_point, root_layer(), func, &data_for_recursion);
  return data_for_recursion.closest_match;
}

void LayerTreeImpl::RegisterSelection(const LayerSelectionBound& start,
                                      const LayerSelectionBound& end) {
  selection_start_ = start;
  selection_end_ = end;
}

static ViewportSelectionBound ComputeViewportSelection(
    const LayerSelectionBound& layer_bound,
    LayerImpl* layer,
    float device_scale_factor) {
  ViewportSelectionBound viewport_bound;
  viewport_bound.type = layer_bound.type;

  if (!layer || layer_bound.type == SELECTION_BOUND_EMPTY)
    return viewport_bound;

  gfx::PointF layer_scaled_top = gfx::ScalePoint(layer_bound.edge_top,
                                                 layer->contents_scale_x(),
                                                 layer->contents_scale_y());
  gfx::PointF layer_scaled_bottom = gfx::ScalePoint(layer_bound.edge_bottom,
                                                    layer->contents_scale_x(),
                                                    layer->contents_scale_y());

  bool clipped = false;
  gfx::PointF screen_top = MathUtil::MapPoint(
      layer->screen_space_transform(), layer_scaled_top, &clipped);
  gfx::PointF screen_bottom = MathUtil::MapPoint(
      layer->screen_space_transform(), layer_scaled_bottom, &clipped);

  const float inv_scale = 1.f / device_scale_factor;
  viewport_bound.edge_top = gfx::ScalePoint(screen_top, inv_scale);
  viewport_bound.edge_bottom = gfx::ScalePoint(screen_bottom, inv_scale);

  // The bottom edge point is used for visibility testing as it is the logical
  // focal point for bound selection handles (this may change in the future).
  // Shifting the visibility point fractionally inward ensures that neighboring
  // or logically coincident layers aligned to integral DPI coordinates will not
  // spuriously occlude the bound.
  gfx::Vector2dF visibility_offset = layer_scaled_top - layer_scaled_bottom;
  visibility_offset.Scale(device_scale_factor / visibility_offset.Length());
  gfx::PointF visibility_point = layer_scaled_bottom + visibility_offset;
  if (visibility_point.x() <= 0)
    visibility_point.set_x(visibility_point.x() + device_scale_factor);
  visibility_point = MathUtil::MapPoint(
      layer->screen_space_transform(), visibility_point, &clipped);

  float intersect_distance = 0.f;
  viewport_bound.visible =
      PointHitsLayer(layer, visibility_point, &intersect_distance);

  return viewport_bound;
}

void LayerTreeImpl::GetViewportSelection(ViewportSelectionBound* start,
                                         ViewportSelectionBound* end) {
  DCHECK(start);
  DCHECK(end);

  *start = ComputeViewportSelection(
      selection_start_,
      selection_start_.layer_id ? LayerById(selection_start_.layer_id) : NULL,
      device_scale_factor());
  if (start->type == SELECTION_BOUND_CENTER ||
      start->type == SELECTION_BOUND_EMPTY) {
    *end = *start;
  } else {
    *end = ComputeViewportSelection(
        selection_end_,
        selection_end_.layer_id ? LayerById(selection_end_.layer_id) : NULL,
        device_scale_factor());
  }
}

void LayerTreeImpl::InputScrollAnimationFinished() {
  layer_tree_host_impl_->ScrollEnd();
}

bool LayerTreeImpl::SmoothnessTakesPriority() const {
  return layer_tree_host_impl_->GetTreePriority() == SMOOTHNESS_TAKES_PRIORITY;
}

BlockingTaskRunner* LayerTreeImpl::BlockingMainThreadTaskRunner() const {
  return proxy()->blocking_main_thread_task_runner();
}

void LayerTreeImpl::SetPendingPageScaleAnimation(
    scoped_ptr<PendingPageScaleAnimation> pending_animation) {
  pending_page_scale_animation_ = pending_animation.Pass();
}

scoped_ptr<PendingPageScaleAnimation>
    LayerTreeImpl::TakePendingPageScaleAnimation() {
  return pending_page_scale_animation_.Pass();
}

}  // namespace cc
