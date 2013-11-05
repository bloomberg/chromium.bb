// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_impl.h"

#include "base/debug/trace_event.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      hud_layer_(0),
      root_scroll_layer_(NULL),
      currently_scrolling_layer_(NULL),
      root_layer_scroll_offset_delegate_(NULL),
      background_color_(0),
      has_transparent_background_(false),
      page_scale_layer_(NULL),
      inner_viewport_scroll_layer_(NULL),
      outer_viewport_scroll_layer_(NULL),
      page_scale_factor_(1),
      page_scale_delta_(1),
      sent_page_scale_delta_(1),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      scrolling_layer_id_from_previous_tree_(0),
      contents_textures_purged_(false),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true),
      next_activation_forces_redraw_(false) {
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  root_layer_.reset();
}

static LayerImpl* FindRootScrollLayerRecursive(LayerImpl* layer) {
  if (!layer)
    return NULL;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    LayerImpl* found = FindRootScrollLayerRecursive(layer->children()[i]);
    if (found)
      return found;
  }

  return NULL;
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  if (root_scroll_layer_)
    root_scroll_layer_->SetScrollOffsetDelegate(NULL);
  root_layer_ = layer.Pass();
  currently_scrolling_layer_ = NULL;
  root_scroll_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree();
}

void LayerTreeImpl::FindRootScrollLayer() {
  root_scroll_layer_ = FindRootScrollLayerRecursive(root_layer_.get());

  if (root_scroll_layer_) {
    UpdateMaxScrollOffset();
    root_scroll_layer_->SetScrollOffsetDelegate(
        root_layer_scroll_offset_delegate_);
  }

  if (scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::FindLayerInSubtree(
        root_layer_.get(),
        scrolling_layer_id_from_previous_tree_);
  }

  scrolling_layer_id_from_previous_tree_ = 0;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  if (root_scroll_layer_)
    root_scroll_layer_->SetScrollOffsetDelegate(NULL);
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

void LayerTreeImpl::PushPropertiesTo(LayerTreeImpl* target_tree) {
  // The request queue should have been processed and does not require a push.
  DCHECK_EQ(ui_resource_request_queue_.size(), 0u);

  if (next_activation_forces_redraw_) {
    layer_tree_host_impl_->SetFullRootLayerDamage();
    next_activation_forces_redraw_ = false;
  }

  target_tree->SetLatencyInfo(latency_info_);
  latency_info_.Clear();
  target_tree->SetPageScaleFactorAndLimits(
      page_scale_factor(), min_page_scale_factor(), max_page_scale_factor());
  target_tree->SetPageScaleDelta(
      target_tree->page_scale_delta() / target_tree->sent_page_scale_delta());
  target_tree->set_sent_page_scale_delta(1);

  if (settings().use_pinch_virtual_viewport) {
    target_tree->SetViewportLayersFromIds(
        page_scale_layer_->id(),
        inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                     : Layer::INVALID_ID);
  }
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
}

LayerImpl* LayerTreeImpl::RootScrollLayer() const {
  return root_scroll_layer_;
}

LayerImpl* LayerTreeImpl::RootContainerLayer() const {
  return root_scroll_layer_ ? root_scroll_layer_->parent() : NULL;
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
    currently_scrolling_layer_->scrollbar_animation_controller()->
        DidScrollGestureEnd(CurrentPhysicalTimeTicks());
  currently_scrolling_layer_ = layer;
  if (layer && layer->scrollbar_animation_controller())
    layer->scrollbar_animation_controller()->DidScrollGestureBegin();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  SetCurrentlyScrollingLayer(NULL);
  scrolling_layer_id_from_previous_tree_ = 0;
}

void LayerTreeImpl::SetPageScaleFactorAndLimits(float page_scale_factor,
    float min_page_scale_factor, float max_page_scale_factor) {
  if (!page_scale_factor)
    return;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  page_scale_factor_ = page_scale_factor;

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->SetTotalPageScaleFactor(
        total_page_scale_factor());
  }
}

void LayerTreeImpl::SetPageScaleDelta(float delta) {
  // Clamp to the current min/max limits.
  float total = page_scale_factor_ * delta;
  if (min_page_scale_factor_ && total < min_page_scale_factor_)
    delta = min_page_scale_factor_ / page_scale_factor_;
  else if (max_page_scale_factor_ && total > max_page_scale_factor_)
    delta = max_page_scale_factor_ / page_scale_factor_;

  if (delta == page_scale_delta_)
    return;

  page_scale_delta_ = delta;

  if (IsActiveTree()) {
    LayerTreeImpl* pending_tree = layer_tree_host_impl_->pending_tree();
    if (pending_tree) {
      DCHECK_EQ(1, pending_tree->sent_page_scale_delta());
      pending_tree->SetPageScaleDelta(
          page_scale_delta_ / sent_page_scale_delta_);
    }
  }

  UpdateMaxScrollOffset();
  set_needs_update_draw_properties();

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->SetTotalPageScaleFactor(
        total_page_scale_factor());
  }
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  return gfx::ScaleSize(layer_tree_host_impl_->UnscaledScrollableViewportSize(),
                        1.0f / total_page_scale_factor());
}

void LayerTreeImpl::UpdateMaxScrollOffset() {
  LayerImpl* root_scroll = RootScrollLayer();
  if (!root_scroll || !root_scroll->children().size())
    return;

  gfx::Vector2dF max_scroll = gfx::Rect(ScrollableSize()).bottom_right() -
      gfx::RectF(ScrollableViewportSize()).bottom_right();

  // The viewport may be larger than the contents in some cases, such as
  // having a vertical scrollbar but no horizontal overflow.
  max_scroll.SetToMax(gfx::Vector2dF());

  root_scroll_layer_->SetMaxScrollOffset(gfx::ToFlooredVector2d(max_scroll));
}

static void ApplySentScrollDeltasFromAbortedCommitTo(LayerImpl* layer) {
  layer->ApplySentScrollDeltasFromAbortedCommit();
}

void LayerTreeImpl::ApplySentScrollAndScaleDeltasFromAbortedCommit() {
  DCHECK(IsActiveTree());

  page_scale_factor_ *= sent_page_scale_delta_;
  page_scale_delta_ /= sent_page_scale_delta_;
  sent_page_scale_delta_ = 1.f;

  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), base::Bind(&ApplySentScrollDeltasFromAbortedCommitTo));
}

static void ApplyScrollDeltasSinceBeginMainFrameTo(LayerImpl* layer) {
  layer->ApplyScrollDeltasSinceBeginMainFrame();
}

void LayerTreeImpl::ApplyScrollDeltasSinceBeginMainFrame() {
  DCHECK(IsPendingTree());
  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(), base::Bind(&ApplyScrollDeltasSinceBeginMainFrameTo));
}

void LayerTreeImpl::SetViewportLayersFromIds(
    int page_scale_layer_id,
    int inner_viewport_scroll_layer_id,
    int outer_viewport_scroll_layer_id) {
  page_scale_layer_ = LayerById(page_scale_layer_id);
  DCHECK(page_scale_layer_);

  inner_viewport_scroll_layer_ =
      LayerById(inner_viewport_scroll_layer_id);
  DCHECK(inner_viewport_scroll_layer_);

  outer_viewport_scroll_layer_ =
      LayerById(outer_viewport_scroll_layer_id);
  DCHECK(outer_viewport_scroll_layer_ ||
         outer_viewport_scroll_layer_id == Layer::INVALID_ID);
}

void LayerTreeImpl::ClearViewportLayers() {
  page_scale_layer_ = NULL;
  inner_viewport_scroll_layer_ = NULL;
  outer_viewport_scroll_layer_ = NULL;
}

// TODO(wjmaclean) This needs to go away, and be replaced with a single core
// of login that works for both scrollbar layer types. This is already planned
// as part of the larger pinch-zoom re-factoring viewport.
void LayerTreeImpl::UpdateSolidColorScrollbars() {
  LayerImpl* root_scroll = RootScrollLayer();
  DCHECK(root_scroll);
  DCHECK(IsActiveTree());

  gfx::RectF scrollable_viewport(
      gfx::PointAtOffsetFromOrigin(root_scroll->TotalScrollOffset()),
      ScrollableViewportSize());
  float vertical_adjust = 0.0f;
  if (RootContainerLayer())
    vertical_adjust =
        layer_tree_host_impl_->UnscaledScrollableViewportSize().height() -
        RootContainerLayer()->bounds().height();
  if (ScrollbarLayerImplBase* horiz =
          root_scroll->horizontal_scrollbar_layer()) {
    horiz->SetVerticalAdjust(vertical_adjust);
    horiz->SetVisibleToTotalLengthRatio(
        scrollable_viewport.width() / ScrollableSize().width());
  }
  if (ScrollbarLayerImplBase* vertical =
          root_scroll->vertical_scrollbar_layer()) {
    vertical->SetVerticalAdjust(vertical_adjust);
    vertical->SetVisibleToTotalLengthRatio(
        scrollable_viewport.height() / ScrollableSize().height());
  }
}

void LayerTreeImpl::UpdateDrawProperties() {
  if (IsActiveTree() && RootScrollLayer() && RootContainerLayer())
    UpdateRootScrollLayerSizeDelta();

  if (IsActiveTree() &&
      RootContainerLayer() &&
      !RootContainerLayer()->masks_to_bounds()) {
    UpdateSolidColorScrollbars();
  }

  needs_update_draw_properties_ = false;
  render_surface_layer_list_.clear();

  // For max_texture_size.
  if (!layer_tree_host_impl_->renderer())
    return;

  if (!root_layer())
    return;

  {
    TRACE_EVENT2("cc",
                 "LayerTreeImpl::UpdateDrawProperties",
                 "IsActive",
                 IsActiveTree(),
                 "SourceFrameNumber",
                 source_frame_number_);
    LayerImpl* page_scale_layer =
        page_scale_layer_ ? page_scale_layer_ : RootContainerLayer();
    bool can_render_to_separate_surface =
        !output_surface()->ForcedDrawToSoftwareDevice();
    LayerTreeHostCommon::CalcDrawPropsImplInputs inputs(
        root_layer(),
        DrawViewportSize(),
        layer_tree_host_impl_->DrawTransform(),
        device_scale_factor(),
        total_page_scale_factor(),
        page_scale_layer,
        MaxTextureSize(),
        settings().can_use_lcd_text,
        can_render_to_separate_surface,
        settings().layer_transforms_should_scale_layer_contents,
        &render_surface_layer_list_);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);
  }

  {
    TRACE_EVENT2("cc",
                 "LayerTreeImpl::UpdateTilePriorities",
                 "IsActive",
                 IsActiveTree(),
                 "SourceFrameNumber",
                 source_frame_number_);
    // LayerIterator is used here instead of CallFunctionForSubtree to only
    // UpdateTilePriorities on layers that will be visible (and thus have valid
    // draw properties) and not because any ordering is required.
    typedef LayerIterator<LayerImpl,
                          LayerImplList,
                          RenderSurfaceImpl,
                          LayerIteratorActions::FrontToBack> LayerIteratorType;
    LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
    for (LayerIteratorType it =
             LayerIteratorType::Begin(&render_surface_layer_list_);
         it != end;
         ++it) {
      if (!it.represents_itself())
        continue;
      LayerImpl* layer = *it;
      if (layer->draw_properties().skip_drawing)
        continue;

      layer->UpdateTilePriorities();
      if (layer->mask_layer())
        layer->mask_layer()->UpdateTilePriorities();
      if (layer->replica_layer() && layer->replica_layer()->mask_layer())
        layer->replica_layer()->mask_layer()->UpdateTilePriorities();
    }
  }

  DCHECK(!needs_update_draw_properties_) <<
      "CalcDrawProperties should not set_needs_update_draw_properties()";
}

const LayerImplList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

gfx::Size LayerTreeImpl::ScrollableSize() const {
  if (!root_scroll_layer_ || root_scroll_layer_->children().empty())
    return gfx::Size();
  return root_scroll_layer_->children()[0]->bounds();
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

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pending_tree) {
  pending_tree->SetCurrentlyScrollingLayer(
      LayerTreeHostCommon::FindLayerInSubtree(pending_tree->root_layer(),
          currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0));
  pending_tree->SetLatencyInfo(latency_info_);
  latency_info_.Clear();
}

static void DidBecomeActiveRecursive(LayerImpl* layer) {
  layer->DidBecomeActive();
  for (size_t i = 0; i < layer->children().size(); ++i)
    DidBecomeActiveRecursive(layer->children()[i]);
}

void LayerTreeImpl::DidBecomeActive() {
  if (!root_layer())
    return;

  DidBecomeActiveRecursive(root_layer());
  FindRootScrollLayer();
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

const RendererCapabilities& LayerTreeImpl::GetRendererCapabilities() const {
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

bool LayerTreeImpl::device_viewport_valid_for_tile_management() const {
  return layer_tree_host_impl_->device_viewport_valid_for_tile_management();
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

int LayerTreeImpl::MaxTextureSize() const {
  return layer_tree_host_impl_->GetRendererCapabilities().max_texture_size;
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinch_gesture_active();
}

base::TimeTicks LayerTreeImpl::CurrentFrameTimeTicks() const {
  return layer_tree_host_impl_->CurrentFrameTimeTicks();
}

base::Time LayerTreeImpl::CurrentFrameTime() const {
  return layer_tree_host_impl_->CurrentFrameTime();
}

base::TimeTicks LayerTreeImpl::CurrentPhysicalTimeTicks() const {
  return layer_tree_host_impl_->CurrentPhysicalTimeTicks();
}

void LayerTreeImpl::SetNeedsCommit() {
  layer_tree_host_impl_->SetNeedsCommit();
}

gfx::Size LayerTreeImpl::DrawViewportSize() const {
  return layer_tree_host_impl_->DrawViewportSize();
}

void LayerTreeImpl::StartScrollbarAnimation() {
  layer_tree_host_impl_->StartScrollbarAnimation();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->SetNeedsRedraw();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debug_state();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->device_scale_factor();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debug_rect_history();
}

AnimationRegistrar* LayerTreeImpl::animationRegistrar() const {
  return layer_tree_host_impl_->animation_registrar();
}

scoped_ptr<base::Value> LayerTreeImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  TracedValue::MakeDictIntoImplicitSnapshot(
      state.get(), "cc::LayerTreeImpl", this);

  state->Set("root_layer", root_layer_->AsValue().release());

  scoped_ptr<base::ListValue> render_surface_layer_list(new base::ListValue());
  typedef LayerIterator<LayerImpl,
                        LayerImplList,
                        RenderSurfaceImpl,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::Begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (!it.represents_itself())
      continue;
    render_surface_layer_list->Append(TracedValue::CreateIDRef(*it).release());
  }

  state->Set("render_surface_layer_list",
             render_surface_layer_list.release());
  return state.PassAs<base::Value>();
}

void LayerTreeImpl::SetRootLayerScrollOffsetDelegate(
    LayerScrollOffsetDelegate* root_layer_scroll_offset_delegate) {
  if (root_layer_scroll_offset_delegate_ == root_layer_scroll_offset_delegate)
    return;

  root_layer_scroll_offset_delegate_ = root_layer_scroll_offset_delegate;

  if (root_scroll_layer_) {
    root_scroll_layer_->SetScrollOffsetDelegate(
        root_layer_scroll_offset_delegate_);
  }

  if (root_layer_scroll_offset_delegate_) {
    root_layer_scroll_offset_delegate_->SetScrollableSize(ScrollableSize());
    root_layer_scroll_offset_delegate_->SetTotalPageScaleFactor(
        total_page_scale_factor());
  }
}

void LayerTreeImpl::UpdateRootScrollLayerSizeDelta() {
  LayerImpl* root_scroll = RootScrollLayer();
  LayerImpl* root_container = RootContainerLayer();
  DCHECK(root_scroll);
  DCHECK(root_container);
  DCHECK(IsActiveTree());

  gfx::Vector2dF scrollable_viewport_size =
      gfx::RectF(ScrollableViewportSize()).bottom_right() - gfx::PointF();

  gfx::Vector2dF original_viewport_size =
      gfx::RectF(root_container->bounds()).bottom_right() -
      gfx::PointF();
  original_viewport_size.Scale(1 / page_scale_factor());

  root_scroll->SetFixedContainerSizeDelta(
      scrollable_viewport_size - original_viewport_size);
}

void LayerTreeImpl::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  latency_info_.MergeWith(latency_info);
}

const ui::LatencyInfo& LayerTreeImpl::GetLatencyInfo() {
  return latency_info_;
}

void LayerTreeImpl::ClearLatencyInfo() {
  latency_info_.Clear();
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
  while (ui_resource_request_queue_.size() > 0) {
    UIResourceRequest req = ui_resource_request_queue_.front();
    ui_resource_request_queue_.pop_front();

    switch (req.GetType()) {
      case UIResourceRequest::UIResourceCreate:
        layer_tree_host_impl_->CreateUIResource(req.GetId(), req.GetBitmap());
        break;
      case UIResourceRequest::UIResourceDelete:
        layer_tree_host_impl_->DeleteUIResource(req.GetId());
        break;
      case UIResourceRequest::UIResourceInvalidRequest:
        NOTREACHED();
        break;
    }
  }

  // If all UI resource evictions were not recreated by processing this queue,
  // then another commit is required.
  if (layer_tree_host_impl_->EvictedUIResourcesExist())
    layer_tree_host_impl_->SetNeedsCommit();
}

void LayerTreeImpl::AddLayerWithCopyOutputRequest(LayerImpl* layer) {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  DCHECK(std::find(layers_with_copy_output_request_.begin(),
                   layers_with_copy_output_request_.end(),
                   layer) == layers_with_copy_output_request_.end());
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
}

const std::vector<LayerImpl*> LayerTreeImpl::LayersWithCopyOutputRequest()
    const {
  // Only the active tree needs to know about layers with copy requests, as
  // they are aborted if not serviced during draw.
  DCHECK(IsActiveTree());

  return layers_with_copy_output_request_;
}

}  // namespace cc
