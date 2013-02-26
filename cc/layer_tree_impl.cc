// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_impl.h"

#include "base/debug/trace_event.h"
#include "cc/heads_up_display_layer_impl.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl),
      source_frame_number_(-1),
      hud_layer_(0),
      root_scroll_layer_(0),
      currently_scrolling_layer_(0),
      background_color_(0),
      has_transparent_background_(false),
      page_scale_factor_(1),
      page_scale_delta_(1),
      sent_page_scale_delta_(1),
      min_page_scale_factor_(0),
      max_page_scale_factor_(0),
      scrolling_layer_id_from_previous_tree_(0),
      contents_textures_purged_(false),
      viewport_size_invalid_(false),
      needs_update_draw_properties_(true),
      needs_full_tree_sync_(true) {
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  root_layer_.reset();
}

static LayerImpl* findRootScrollLayer(LayerImpl* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerImpl* found = findRootScrollLayer(layer->children()[i]);
        if (found)
            return found;
    }

    return 0;
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  root_layer_ = layer.Pass();
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

void LayerTreeImpl::FindRootScrollLayer() {
  root_scroll_layer_ = findRootScrollLayer(root_layer_.get());

  if (root_layer_ && scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::findLayerInSubtree(
        root_layer_.get(),
        scrolling_layer_id_from_previous_tree_);
  }

  scrolling_layer_id_from_previous_tree_ = 0;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

void LayerTreeImpl::pushPropertiesTo(LayerTreeImpl* target_tree) {
  target_tree->SetPageScaleFactorAndLimits(
      page_scale_factor(), min_page_scale_factor(), max_page_scale_factor());
  target_tree->SetPageScaleDelta(
      target_tree->page_scale_delta() / target_tree->sent_page_scale_delta());
  target_tree->set_sent_page_scale_delta(1);

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
        LayerTreeHostCommon::findLayerInSubtree(
            target_tree->RootLayer(), hud_layer()->id())));
  else
    target_tree->set_hud_layer(NULL);
}

LayerImpl* LayerTreeImpl::RootScrollLayer() {
  DCHECK(IsActiveTree());
  return root_scroll_layer_;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() {
  DCHECK(IsActiveTree());
  return currently_scrolling_layer_;
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  currently_scrolling_layer_ = NULL;
  scrolling_layer_id_from_previous_tree_ = 0;
}

void LayerTreeImpl::SetPageScaleFactorAndLimits(float page_scale_factor,
    float min_page_scale_factor, float max_page_scale_factor)
{
  if (!page_scale_factor)
    return;

  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  page_scale_factor_ = page_scale_factor;
}

void LayerTreeImpl::SetPageScaleDelta(float delta)
{
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
    LayerTreeImpl* pending_tree = layer_tree_host_impl_->pendingTree();
    if (pending_tree) {
      DCHECK_EQ(1, pending_tree->sent_page_scale_delta());
      pending_tree->SetPageScaleDelta(page_scale_delta_ / sent_page_scale_delta_);
    }
  }

  UpdateMaxScrollOffset();
  set_needs_update_draw_properties();
}

gfx::SizeF LayerTreeImpl::ScrollableViewportSize() const {
  gfx::SizeF view_bounds;
  // The clip layer should be used for scrolling bounds if available since it
  // adjusts for non-overlay scrollbars. Otherwise, fall back to our knowledge
  // of the physical viewport size.
  LayerImpl* clip_layer = NULL;
  if (root_scroll_layer_)
    clip_layer = root_scroll_layer_->parent();
  if (clip_layer && clip_layer->masksToBounds()) {
    view_bounds = clip_layer->bounds();
  } else {
    view_bounds = gfx::ScaleSize(device_viewport_size(),
        1 / device_scale_factor());
  }
  view_bounds.Scale(1 / total_page_scale_factor());

  return view_bounds;
}

void LayerTreeImpl::UpdateMaxScrollOffset() {
  if (!root_scroll_layer_ || !root_scroll_layer_->children().size())
    return;

  gfx::Vector2dF max_scroll = gfx::Rect(ScrollableSize()).bottom_right() -
      gfx::RectF(ScrollableViewportSize()).bottom_right();

  // The viewport may be larger than the contents in some cases, such as
  // having a vertical scrollbar but no horizontal overflow.
  max_scroll.ClampToMin(gfx::Vector2dF());

  root_scroll_layer_->setMaxScrollOffset(gfx::ToFlooredVector2d(max_scroll));
}

gfx::Transform LayerTreeImpl::ImplTransform() const {
  gfx::Transform transform;
  transform.Scale(total_page_scale_factor(), total_page_scale_factor());
  return transform;
}

struct UpdateTilePrioritiesForLayer {
  void operator()(LayerImpl *layer) {
    layer->updateTilePriorities();
  }
};

void LayerTreeImpl::UpdateDrawProperties(UpdateDrawPropertiesReason reason) {
  if (!needs_update_draw_properties_) {
    if (reason == UPDATE_ACTIVE_TREE_FOR_DRAW && RootLayer())
      LayerTreeHostCommon::callFunctionForSubtree<UpdateTilePrioritiesForLayer>(
          RootLayer());
    return;
  }

  needs_update_draw_properties_ = false;
  render_surface_layer_list_.clear();

  // For maxTextureSize.
  if (!layer_tree_host_impl_->renderer())
      return;

  if (!RootLayer())
    return;

  if (root_scroll_layer_) {
    root_scroll_layer_->setImplTransform(ImplTransform());
    // Setting the impl transform re-sets this.
    needs_update_draw_properties_ = false;
  }

  {
    TRACE_EVENT1("cc", "LayerTreeImpl::UpdateDrawProperties", "IsActive", IsActiveTree());
    bool update_tile_priorities =
        reason == UPDATE_PENDING_TREE ||
        reason == UPDATE_ACTIVE_TREE_FOR_DRAW;
    LayerTreeHostCommon::calculateDrawProperties(
        RootLayer(),
        device_viewport_size(),
        device_scale_factor(),
        total_page_scale_factor(),
        MaxTextureSize(),
        settings().canUseLCDText,
        render_surface_layer_list_,
        update_tile_priorities);
  }

  DCHECK(!needs_update_draw_properties_) <<
      "calcDrawProperties should not set_needs_update_draw_properties()";
}

static void ClearRenderSurfacesOnLayerImplRecursive(LayerImpl* current)
{
    DCHECK(current);
    for (size_t i = 0; i < current->children().size(); ++i)
        ClearRenderSurfacesOnLayerImplRecursive(current->children()[i]);
    current->clearRenderSurface();
}

void LayerTreeImpl::ClearRenderSurfaces() {
  ClearRenderSurfacesOnLayerImplRecursive(RootLayer());
  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
}

bool LayerTreeImpl::AreVisibleResourcesReady() const {
  TRACE_EVENT0("cc", "LayerTreeImpl::AreVisibleResourcesReady");

  typedef LayerIterator<LayerImpl,
                        std::vector<LayerImpl*>,
                        RenderSurfaceImpl,
                        LayerIteratorActions::BackToFront> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::end(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (it.representsItself() && !(*it)->areVisibleResourcesReady())
      return false;
  }

  return true;
}

const LayerTreeImpl::LayerList& LayerTreeImpl::RenderSurfaceLayerList() const {
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

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pendingTree) {
  int id = currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  pendingTree->set_currently_scrolling_layer(
      LayerTreeHostCommon::findLayerInSubtree(pendingTree->RootLayer(), id));
}

static void DidBecomeActiveRecursive(LayerImpl* layer) {
  layer->didBecomeActive();
  for (size_t i = 0; i < layer->children().size(); ++i)
    DidBecomeActiveRecursive(layer->children()[i]);
}

void LayerTreeImpl::DidBecomeActive() {
  if (RootLayer())
    DidBecomeActiveRecursive(RootLayer());
  FindRootScrollLayer();
  UpdateMaxScrollOffset();
}

bool LayerTreeImpl::ContentsTexturesPurged() const {
  return contents_textures_purged_;
}

void LayerTreeImpl::SetContentsTexturesPurged() {
  contents_textures_purged_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

void LayerTreeImpl::ResetContentsTexturesPurged() {
  contents_textures_purged_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

bool LayerTreeImpl::ViewportSizeInvalid() const {
  return viewport_size_invalid_;
}

void LayerTreeImpl::SetViewportSizeInvalid() {
  viewport_size_invalid_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

void LayerTreeImpl::ResetViewportSizeInvalid() {
  viewport_size_invalid_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

Proxy* LayerTreeImpl::proxy() const {
  return layer_tree_host_impl_->proxy();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
}

const RendererCapabilities& LayerTreeImpl::rendererCapabilities() const {
  return layer_tree_host_impl_->rendererCapabilities();
}

OutputSurface* LayerTreeImpl::output_surface() const {
  return layer_tree_host_impl_->outputSurface();
}

ResourceProvider* LayerTreeImpl::resource_provider() const {
  return layer_tree_host_impl_->resourceProvider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return layer_tree_host_impl_->tileManager();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fpsCounter();
}

PaintTimeCounter* LayerTreeImpl::paint_time_counter() const {
  return layer_tree_host_impl_->paintTimeCounter();
}

MemoryHistory* LayerTreeImpl::memory_history() const {
  return layer_tree_host_impl_->memoryHistory();
}

bool LayerTreeImpl::IsActiveTree() const {
  return layer_tree_host_impl_->activeTree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return layer_tree_host_impl_->pendingTree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return layer_tree_host_impl_->recycleTree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->activeTree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->pendingTree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

int LayerTreeImpl::MaxTextureSize() const {
  return layer_tree_host_impl_->rendererCapabilities().maxTextureSize;
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinchGestureActive();
}

base::TimeTicks LayerTreeImpl::CurrentFrameTime() const {
  return layer_tree_host_impl_->currentFrameTime();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->setNeedsRedraw();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debugState();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->deviceScaleFactor();
}

const gfx::Size& LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->deviceViewportSize();
}

const gfx::Size& LayerTreeImpl::layout_viewport_size() const {
  return layer_tree_host_impl_->layoutViewportSize();
}

std::string LayerTreeImpl::layer_tree_as_text() const {
  return layer_tree_host_impl_->layerTreeAsText();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debugRectHistory();
}

AnimationRegistrar* LayerTreeImpl::animationRegistrar() const {
  return layer_tree_host_impl_->animationRegistrar();
}

scoped_ptr<base::Value> LayerTreeImpl::AsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  typedef LayerIterator<LayerImpl,
                        std::vector<LayerImpl*>,
                        RenderSurfaceImpl,
                        LayerIteratorActions::BackToFront> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::end(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (!it.representsItself())
      continue;
    state->Append((*it)->AsValue().release());
  }
  return state.PassAs<base::Value>();
}

} // namespace cc
