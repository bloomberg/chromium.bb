// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/base/thread.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/pinch_zoom_scrollbar.h"
#include "cc/input/pinch_zoom_scrollbar_geometry.h"
#include "cc/input/pinch_zoom_scrollbar_painter.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"

namespace {
static int s_num_layer_tree_instances;
}

namespace cc {

RendererCapabilities::RendererCapabilities()
    : best_texture_format(0),
      using_partial_swap(false),
      using_accelerated_painting(false),
      using_set_visibility(false),
      using_swap_complete_callback(false),
      using_gpu_memory_manager(false),
      using_egl_image(false),
      allow_partial_texture_updates(false),
      using_offscreen_context3d(false),
      max_texture_size(0),
      avoid_pow2_textures(false) {}

RendererCapabilities::~RendererCapabilities() {}

bool LayerTreeHost::AnyLayerTreeHostInstanceExists() {
  return s_num_layer_tree_instances > 0;
}

scoped_ptr<LayerTreeHost> LayerTreeHost::Create(
    LayerTreeHostClient* client,
    const LayerTreeSettings& settings,
    scoped_ptr<Thread> impl_thread) {
  scoped_ptr<LayerTreeHost> layer_tree_host(new LayerTreeHost(client,
                                                              settings));
  if (!layer_tree_host->Initialize(impl_thread.Pass()))
    return scoped_ptr<LayerTreeHost>();
  return layer_tree_host.Pass();
}

LayerTreeHost::LayerTreeHost(LayerTreeHostClient* client,
                             const LayerTreeSettings& settings)
    : animating_(false),
      needs_full_tree_sync_(true),
      needs_filter_context_(false),
      client_(client),
      commit_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      renderer_initialized_(false),
      output_surface_lost_(false),
      num_failed_recreate_attempts_(0),
      settings_(settings),
      debug_state_(settings.initial_debug_state),
      device_scale_factor_(1.f),
      visible_(true),
      page_scale_factor_(1.f),
      min_page_scale_factor_(1.f),
      max_page_scale_factor_(1.f),
      overdraw_bottom_height_(0.f),
      trigger_idle_updates_(true),
      background_color_(SK_ColorWHITE),
      has_transparent_background_(false),
      partial_texture_update_requests_(0) {
  if (settings_.accelerated_animation_enabled)
    animation_registrar_ = AnimationRegistrar::Create();
  s_num_layer_tree_instances++;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

bool LayerTreeHost::Initialize(scoped_ptr<Thread> impl_thread) {
  if (impl_thread)
    return InitializeProxy(ThreadProxy::Create(this, impl_thread.Pass()));
  else
    return InitializeProxy(SingleThreadProxy::Create(this));
}

bool LayerTreeHost::InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing) {
  return InitializeProxy(proxy_for_testing.Pass());
}

bool LayerTreeHost::InitializeProxy(scoped_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");

  proxy_ = proxy.Pass();
  proxy_->Start();
  return proxy_->InitializeOutputSurface();
}

LayerTreeHost::~LayerTreeHost() {
  if (root_layer_)
    root_layer_->SetLayerTreeHost(NULL);
  DCHECK(proxy_);
  DCHECK(proxy_->IsMainThread());
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");
  proxy_->Stop();
  s_num_layer_tree_instances--;
  RateLimiterMap::iterator it = rate_limiters_.begin();
  if (it != rate_limiters_.end())
    it->second->Stop();

  if (root_layer_) {
    // The layer tree must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the registrar
    // will outlive them, and we must make good.
    root_layer_ = NULL;
  }
}

void LayerTreeHost::SetSurfaceReady() {
  proxy_->SetSurfaceReady();
}

void LayerTreeHost::InitializeRenderer() {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeRenderer");
  if (!proxy_->InitializeRenderer()) {
    // Uh oh, better tell the client that we can't do anything with this output
    // surface.
    client_->DidRecreateOutputSurface(false);
    return;
  }

  // Update settings_ based on capabilities that we got back from the renderer.
  settings_.accelerate_painting =
      proxy_->GetRendererCapabilities().using_accelerated_painting;

  // Update settings_ based on partial update capability.
  size_t max_partial_texture_updates = 0;
  if (proxy_->GetRendererCapabilities().allow_partial_texture_updates &&
      !settings_.impl_side_painting) {
    max_partial_texture_updates = std::min(
        settings_.max_partial_texture_updates,
        proxy_->MaxPartialTextureUpdates());
  }
  settings_.max_partial_texture_updates = max_partial_texture_updates;

  contents_texture_manager_ = PrioritizedResourceManager::Create(proxy_.get());
  surface_memory_placeholder_ =
      contents_texture_manager_->CreateTexture(gfx::Size(), GL_RGBA);

  renderer_initialized_ = true;

  int max_texture_size = proxy_->GetRendererCapabilities().max_texture_size;
  settings_.default_tile_size = gfx::Size(
      std::min(settings_.default_tile_size.width(), max_texture_size),
      std::min(settings_.default_tile_size.height(), max_texture_size));
  settings_.max_untiled_layer_size = gfx::Size(
      std::min(settings_.max_untiled_layer_size.width(), max_texture_size),
      std::min(settings_.max_untiled_layer_size.height(), max_texture_size));
}

LayerTreeHost::RecreateResult LayerTreeHost::RecreateOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::RecreateOutputSurface");
  DCHECK(output_surface_lost_);

  if (proxy_->RecreateOutputSurface()) {
    client_->DidRecreateOutputSurface(true);
    output_surface_lost_ = false;
    return RecreateSucceeded;
  }

  client_->WillRetryRecreateOutputSurface();

  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  num_failed_recreate_attempts_++;
  if (num_failed_recreate_attempts_ < 5) {
    // FIXME: The single thread does not self-schedule output surface
    // recreation. So force another recreation attempt to happen by requesting
    // another commit.
    if (!proxy_->HasImplThread())
      SetNeedsCommit();
    return RecreateFailedButTryAgain;
  }

  // We have tried too many times to recreate the output surface. Tell the
  // host to fall back to software rendering.
  client_->DidRecreateOutputSurface(false);
  return RecreateFailedAndGaveUp;
}

void LayerTreeHost::DeleteContentsTexturesOnImplThread(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  if (renderer_initialized_)
    contents_texture_manager_->ClearAllMemory(resource_provider);
}

void LayerTreeHost::AcquireLayerTextures() {
  DCHECK(proxy_->IsMainThread());
  proxy_->AcquireLayerTextures();
}

void LayerTreeHost::DidBeginFrame() {
  client_->DidBeginFrame();
}

void LayerTreeHost::UpdateAnimations(base::TimeTicks frame_begin_time) {
  animating_ = true;
  client_->Animate((frame_begin_time - base::TimeTicks()).InSecondsF());
  AnimateLayers(frame_begin_time);
  animating_ = false;

  rendering_stats_instrumentation_->IncrementAnimationFrameCount();
}

void LayerTreeHost::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

void LayerTreeHost::Layout() {
  client_->Layout();
}

void LayerTreeHost::BeginCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHost::CommitTo");
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(proxy_->IsImplThread());

  // If there are linked evicted backings, these backings' resources may be put
  // into the impl tree, so we can't draw yet. Determine this before clearing
  // all evicted backings.
  bool new_impl_tree_has_no_evicted_resources =
      !contents_texture_manager_->LinkedEvictedBackingsExist();

  contents_texture_manager_->UpdateBackingsInDrawingImplTree();

  // In impl-side painting, synchronize to the pending tree so that it has
  // time to raster before being displayed.  If no pending tree is needed,
  // synchronization can happen directly to the active tree and
  // unlinked contents resources can be reclaimed immediately.
  LayerTreeImpl* sync_tree;
  if (settings_.impl_side_painting) {
    // Commits should not occur while there is already a pending tree.
    DCHECK(!host_impl->pending_tree());
    host_impl->CreatePendingTree();
    sync_tree = host_impl->pending_tree();
  } else {
    contents_texture_manager_->ReduceMemory(host_impl->resource_provider());
    sync_tree = host_impl->active_tree();
  }

  if (needs_full_tree_sync_)
    sync_tree->SetRootLayer(TreeSynchronizer::SynchronizeTrees(
        root_layer(), sync_tree->DetachLayerTree(), sync_tree)); {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");
    TreeSynchronizer::PushProperties(root_layer(), sync_tree->root_layer());
  }

  sync_tree->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (root_layer_ && hud_layer_) {
    LayerImpl* hud_impl = LayerTreeHostCommon::FindLayerInSubtree(
        sync_tree->root_layer(), hud_layer_->id());
    sync_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    sync_tree->set_hud_layer(NULL);
  }

  sync_tree->set_source_frame_number(commit_number());
  sync_tree->set_background_color(background_color_);
  sync_tree->set_has_transparent_background(has_transparent_background_);

  sync_tree->FindRootScrollLayer();

  float page_scale_delta, sent_page_scale_delta;
  if (settings_.impl_side_painting) {
    // Update the delta from the active tree, which may have
    // adjusted its delta prior to the pending tree being created.
    // This code is equivalent to that in LayerTreeImpl::SetPageScaleDelta.
    DCHECK_EQ(1.f, sync_tree->sent_page_scale_delta());
    page_scale_delta = host_impl->active_tree()->page_scale_delta();
    sent_page_scale_delta = host_impl->active_tree()->sent_page_scale_delta();
  } else {
    page_scale_delta = sync_tree->page_scale_delta();
    sent_page_scale_delta = sync_tree->sent_page_scale_delta();
    sync_tree->set_sent_page_scale_delta(1.f);
  }

  sync_tree->SetPageScaleFactorAndLimits(page_scale_factor_,
                                         min_page_scale_factor_,
                                         max_page_scale_factor_);
  sync_tree->SetPageScaleDelta(page_scale_delta / sent_page_scale_delta);

  host_impl->SetViewportSize(layout_viewport_size_, device_viewport_size_);
  host_impl->SetOverdrawBottomHeight(overdraw_bottom_height_);
  host_impl->SetDeviceScaleFactor(device_scale_factor_);
  host_impl->SetDebugState(debug_state_);

  DCHECK(!sync_tree->ViewportSizeInvalid());

  if (new_impl_tree_has_no_evicted_resources) {
    if (sync_tree->ContentsTexturesPurged())
      sync_tree->ResetContentsTexturesPurged();
  }

  sync_tree->SetPinchZoomHorizontalLayerId(
      pinch_zoom_scrollbar_horizontal_ ?
          pinch_zoom_scrollbar_horizontal_->id() : Layer::INVALID_ID);

  sync_tree->SetPinchZoomVerticalLayerId(
      pinch_zoom_scrollbar_vertical_ ?
          pinch_zoom_scrollbar_vertical_->id() : Layer::INVALID_ID);

  if (!settings_.impl_side_painting) {
    // If we're not in impl-side painting, the tree is immediately
    // considered active.
    sync_tree->DidBecomeActive();
  }

  commit_number_++;
}

void LayerTreeHost::SetPinchZoomScrollbarsBoundsAndPosition() {
  if (!pinch_zoom_scrollbar_horizontal_ || !pinch_zoom_scrollbar_vertical_)
    return;

  gfx::Size size = layout_viewport_size();
  int track_width = PinchZoomScrollbarGeometry::kTrackWidth;

  pinch_zoom_scrollbar_horizontal_->SetBounds(
      gfx::Size(size.width() - track_width, track_width));
  pinch_zoom_scrollbar_horizontal_->SetPosition(
      gfx::PointF(0, size.height() - track_width));

  pinch_zoom_scrollbar_vertical_->SetBounds(
      gfx::Size(track_width, size.height() - track_width));
  pinch_zoom_scrollbar_vertical_->SetPosition(
      gfx::PointF(size.width() - track_width, 0));
}

static scoped_refptr<ScrollbarLayer> CreatePinchZoomScrollbar(
    WebKit::WebScrollbar::Orientation orientation,
    LayerTreeHost* owner) {
  scoped_refptr<ScrollbarLayer> scrollbar_layer = ScrollbarLayer::Create(
      make_scoped_ptr(new PinchZoomScrollbar(orientation, owner)).
          PassAs<WebKit::WebScrollbar>(),
      scoped_ptr<ScrollbarThemePainter>(new PinchZoomScrollbarPainter).Pass(),
      scoped_ptr<WebKit::WebScrollbarThemeGeometry>(
          new PinchZoomScrollbarGeometry).Pass(),
      Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID);
  scrollbar_layer->SetIsDrawable(true);
  scrollbar_layer->SetOpacity(0.f);
  return scrollbar_layer;
}

void LayerTreeHost::CreateAndAddPinchZoomScrollbars() {
  bool needs_properties_updated = false;

  if (!pinch_zoom_scrollbar_horizontal_ || !pinch_zoom_scrollbar_vertical_) {
    pinch_zoom_scrollbar_horizontal_ =
        CreatePinchZoomScrollbar(WebKit::WebScrollbar::Horizontal, this);
    pinch_zoom_scrollbar_vertical_ =
        CreatePinchZoomScrollbar(WebKit::WebScrollbar::Vertical, this);
    needs_properties_updated = true;
  }

  DCHECK(pinch_zoom_scrollbar_horizontal_ && pinch_zoom_scrollbar_vertical_);

  if (!pinch_zoom_scrollbar_horizontal_->parent())
    root_layer_->AddChild(pinch_zoom_scrollbar_horizontal_);

  if (!pinch_zoom_scrollbar_vertical_->parent())
    root_layer_->AddChild(pinch_zoom_scrollbar_vertical_);

  if (needs_properties_updated)
    SetPinchZoomScrollbarsBoundsAndPosition();
}

void LayerTreeHost::WillCommit() {
  client_->WillCommit();

  if (settings().use_pinch_zoom_scrollbars)
    CreateAndAddPinchZoomScrollbars();
}

void LayerTreeHost::UpdateHudLayer() {
  if (debug_state_.ShowHudInfo()) {
    if (!hud_layer_)
      hud_layer_ = HeadsUpDisplayLayer::Create();

    if (root_layer_ && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = NULL;
  }
}

void LayerTreeHost::CommitComplete() {
  client_->DidCommit();
}

scoped_ptr<OutputSurface> LayerTreeHost::CreateOutputSurface() {
  return client_->CreateOutputSurface();
}

scoped_ptr<InputHandler> LayerTreeHost::CreateInputHandler() {
  return client_->CreateInputHandler();
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(proxy_->IsImplThread());
  scoped_ptr<LayerTreeHostImpl> host_impl =
      LayerTreeHostImpl::Create(settings_,
                                client,
                                proxy_.get(),
                                rendering_stats_instrumentation_.get());
  if (settings_.calculate_top_controls_position &&
      host_impl->top_controls_manager()) {
    top_controls_manager_weak_ptr_ =
        host_impl->top_controls_manager()->AsWeakPtr();
  }
  return host_impl.Pass();
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(proxy_->IsMainThread());
  output_surface_lost_ = true;
  num_failed_recreate_attempts_ = 0;
  SetNeedsCommit();
}

bool LayerTreeHost::CompositeAndReadback(void* pixels,
                                         gfx::Rect rect_in_device_viewport) {
  trigger_idle_updates_ = false;
  bool ret = proxy_->CompositeAndReadback(pixels, rect_in_device_viewport);
  trigger_idle_updates_ = true;
  return ret;
}

void LayerTreeHost::FinishAllRendering() {
  if (!renderer_initialized_)
    return;
  proxy_->FinishAllRendering();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::DidDeferCommit() {}

void LayerTreeHost::CollectRenderingStats(RenderingStats* stats) const {
  CHECK(debug_state_.RecordRenderingStats());
  *stats = rendering_stats_instrumentation_->GetRenderingStats();
}

const RendererCapabilities& LayerTreeHost::GetRendererCapabilities() const {
  return proxy_->GetRendererCapabilities();
}

void LayerTreeHost::SetNeedsAnimate() {
  DCHECK(proxy_->HasImplThread());
  proxy_->SetNeedsAnimate();
}

void LayerTreeHost::SetNeedsCommit() {
  if (!prepaint_callback_.IsCancelled()) {
    TRACE_EVENT_INSTANT0("cc",
                         "LayerTreeHost::SetNeedsCommit::cancel prepaint",
                         TRACE_EVENT_SCOPE_THREAD);
    prepaint_callback_.Cancel();
  }
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedraw() {
  proxy_->SetNeedsRedraw();
  if (!proxy_->ImplThread())
    client_->ScheduleComposite();
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

void LayerTreeHost::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                                       base::Time wall_clock_time) {
  DCHECK(proxy_->IsMainThread());
  SetAnimationEventsRecursive(*events,
                              root_layer_.get(),
                              wall_clock_time);
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_ == root_layer)
    return;

  if (root_layer_)
    root_layer_->SetLayerTreeHost(NULL);
  root_layer_ = root_layer;
  if (root_layer_)
    root_layer_->SetLayerTreeHost(this);

  if (hud_layer_)
    hud_layer_->RemoveFromParent();

  if (pinch_zoom_scrollbar_horizontal_)
    pinch_zoom_scrollbar_horizontal_->RemoveFromParent();

  if (pinch_zoom_scrollbar_vertical_)
    pinch_zoom_scrollbar_vertical_->RemoveFromParent();

  SetNeedsFullTreeSync();
}

void LayerTreeHost::SetDebugState(const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::SetViewportSize(gfx::Size layout_viewport_size,
                                    gfx::Size device_viewport_size) {
  if (layout_viewport_size == layout_viewport_size_ &&
      device_viewport_size == device_viewport_size_)
    return;

  layout_viewport_size_ = layout_viewport_size;
  device_viewport_size_ = device_viewport_size;

  SetPinchZoomScrollbarsBoundsAndPosition();
  SetNeedsCommit();
}

void LayerTreeHost::SetOverdrawBottomHeight(float overdraw_bottom_height) {
  if (overdraw_bottom_height_ == overdraw_bottom_height)
    return;

  overdraw_bottom_height_ = overdraw_bottom_height;
  SetNeedsCommit();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor == page_scale_factor_ &&
      min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetNeedsCommit();
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

void LayerTreeHost::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  proxy_->StartPageScaleAnimation(target_offset, use_anchor, scale, duration);
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time) {
  if (!proxy_->HasImplThread())
    static_cast<SingleThreadProxy*>(proxy_.get())->CompositeImmediately(
        frame_begin_time);
  else
    SetNeedsCommit();
}

void LayerTreeHost::ScheduleComposite() {
  client_->ScheduleComposite();
}

bool LayerTreeHost::InitializeRendererIfNeeded() {
  if (!renderer_initialized_) {
    InitializeRenderer();
    // If we couldn't initialize, then bail since we're returning to software
    // mode.
    if (!renderer_initialized_)
      return false;
  }
  if (output_surface_lost_) {
    if (RecreateOutputSurface() != RecreateSucceeded)
      return false;
  }
  return true;
}

void LayerTreeHost::UpdateLayers(ResourceUpdateQueue* queue,
                                 size_t memory_allocation_limit_bytes) {
  DCHECK(renderer_initialized_);

  if (!root_layer())
    return;

  if (layout_viewport_size().IsEmpty())
    return;

  if (memory_allocation_limit_bytes) {
    contents_texture_manager_->SetMaxMemoryLimitBytes(
        memory_allocation_limit_bytes);
  }

  UpdateLayers(root_layer(), queue);
}

static Layer* FindFirstScrollableLayer(Layer* layer) {
  if (!layer)
    return NULL;

  if (layer->scrollable())
    return layer;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    Layer* found = FindFirstScrollableLayer(layer->children()[i].get());
    if (found)
      return found;
  }

  return NULL;
}

const Layer* LayerTreeHost::RootScrollLayer() const {
  return FindFirstScrollableLayer(root_layer_.get());
}

void LayerTreeHost::UpdateLayers(Layer* root_layer,
                                 ResourceUpdateQueue* queue) {
  TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers");

  LayerList update_list; {
    Layer* root_scroll = FindFirstScrollableLayer(root_layer);
    if (root_scroll)
      root_scroll->SetImplTransform(impl_transform_);

    UpdateHudLayer();

    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::CalcDrawProps");
    LayerTreeHostCommon::CalculateDrawProperties(
        root_layer,
        device_viewport_size(),
        device_scale_factor_,
        page_scale_factor_,
        GetRendererCapabilities().max_texture_size,
        settings_.can_use_lcd_text,
        &update_list);
  }

  // Reset partial texture update requests.
  partial_texture_update_requests_ = 0;

  bool need_more_updates = PaintLayerContents(update_list, queue);
  if (trigger_idle_updates_ && need_more_updates) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::posting prepaint task");
    prepaint_callback_.Reset(base::Bind(&LayerTreeHost::TriggerPrepaint,
                                        base::Unretained(this)));
    static base::TimeDelta prepaint_delay =
        base::TimeDelta::FromMilliseconds(100);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            prepaint_callback_.callback(),
                                            prepaint_delay);
  }

  for (size_t i = 0; i < update_list.size(); ++i)
    update_list[i]->ClearRenderSurface();
}

void LayerTreeHost::TriggerPrepaint() {
  prepaint_callback_.Cancel();
  TRACE_EVENT0("cc", "LayerTreeHost::TriggerPrepaint");
  SetNeedsCommit();
}

void LayerTreeHost::SetPrioritiesForSurfaces(size_t surface_memory_bytes) {
  // Surfaces have a place holder for their memory since they are managed
  // independantly but should still be tracked and reduce other memory usage.
  surface_memory_placeholder_->SetTextureManager(
      contents_texture_manager_.get());
  surface_memory_placeholder_->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());
  surface_memory_placeholder_->SetToSelfManagedMemoryPlaceholder(
      surface_memory_bytes);
}

void LayerTreeHost::SetPrioritiesForLayers(const LayerList& update_list) {
  // Use BackToFront since it's cheap and this isn't order-dependent.
  typedef LayerIterator<Layer,
                        LayerList,
                        RenderSurface,
                        LayerIteratorActions::BackToFront> LayerIteratorType;

  PriorityCalculator calculator;
  LayerIteratorType end = LayerIteratorType::End(&update_list);
  for (LayerIteratorType it = LayerIteratorType::Begin(&update_list);
       it != end;
       ++it) {
    if (it.represents_itself()) {
      it->SetTexturePriorities(calculator);
    } else if (it.represents_target_render_surface()) {
      if (it->mask_layer())
        it->mask_layer()->SetTexturePriorities(calculator);
      if (it->replica_layer() && it->replica_layer()->mask_layer())
        it->replica_layer()->mask_layer()->SetTexturePriorities(calculator);
    }
  }
}

void LayerTreeHost::PrioritizeTextures(
    const LayerList& render_surface_layer_list, OverdrawMetrics* metrics) {
  contents_texture_manager_->ClearPriorities();

  size_t memory_for_render_surfaces_metric =
      CalculateMemoryForRenderSurfaces(render_surface_layer_list);

  SetPrioritiesForLayers(render_surface_layer_list);
  SetPrioritiesForSurfaces(memory_for_render_surfaces_metric);

  metrics->DidUseContentsTextureMemoryBytes(
      contents_texture_manager_->MemoryAboveCutoffBytes());
  metrics->DidUseRenderSurfaceTextureMemoryBytes(
      memory_for_render_surfaces_metric);

  contents_texture_manager_->PrioritizeTextures();
}

size_t LayerTreeHost::CalculateMemoryForRenderSurfaces(
    const LayerList& update_list) {
  size_t readback_bytes = 0;
  size_t max_background_texture_bytes = 0;
  size_t contents_texture_bytes = 0;

  // Start iteration at 1 to skip the root surface as it does not have a texture
  // cost.
  for (size_t i = 1; i < update_list.size(); ++i) {
    Layer* render_surface_layer = update_list[i].get();
    RenderSurface* render_surface = render_surface_layer->render_surface();

    size_t bytes =
        Resource::MemorySizeBytes(render_surface->content_rect().size(),
                                  GL_RGBA);
    contents_texture_bytes += bytes;

    if (render_surface_layer->background_filters().isEmpty())
      continue;

    if (bytes > max_background_texture_bytes)
      max_background_texture_bytes = bytes;
    if (!readback_bytes) {
      readback_bytes = Resource::MemorySizeBytes(device_viewport_size_,
                                                 GL_RGBA);
    }
  }
  return readback_bytes + max_background_texture_bytes + contents_texture_bytes;
}

bool LayerTreeHost::PaintMasksForRenderSurface(Layer* render_surface_layer,
                                               ResourceUpdateQueue* queue,
                                               RenderingStats* stats) {
  // Note: Masks and replicas only exist for layers that own render surfaces. If
  // we reach this point in code, we already know that at least something will
  // be drawn into this render surface, so the mask and replica should be
  // painted.

  bool need_more_updates = false;
  Layer* mask_layer = render_surface_layer->mask_layer();
  if (mask_layer) {
    mask_layer->Update(queue, NULL, stats);
    need_more_updates |= mask_layer->NeedMoreUpdates();
  }

  Layer* replica_mask_layer =
      render_surface_layer->replica_layer() ?
      render_surface_layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    replica_mask_layer->Update(queue, NULL, stats);
    need_more_updates |= replica_mask_layer->NeedMoreUpdates();
  }
  return need_more_updates;
}

bool LayerTreeHost::PaintLayerContents(
    const LayerList& render_surface_layer_list, ResourceUpdateQueue* queue) {
  // Use FrontToBack to allow for testing occlusion and performing culling
  // during the tree walk.
  typedef LayerIterator<Layer,
                        LayerList,
                        RenderSurface,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;

  bool need_more_updates = false;
  bool record_metrics_for_frame =
      settings_.show_overdraw_in_tracing &&
      base::debug::TraceLog::GetInstance() &&
      base::debug::TraceLog::GetInstance()->IsEnabled();
  OcclusionTracker occlusion_tracker(
      root_layer_->render_surface()->content_rect(), record_metrics_for_frame);
  occlusion_tracker.set_minimum_tracking_size(
      settings_.minimum_occlusion_tracking_size);

  PrioritizeTextures(render_surface_layer_list,
                     occlusion_tracker.overdraw_metrics());

  // TODO(egraether): Use RenderingStatsInstrumentation in Layer::update()
  RenderingStats stats;
  RenderingStats* stats_ptr =
      debug_state_.RecordRenderingStats() ? &stats : NULL;

  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    occlusion_tracker.EnterLayer(it);

    if (it.represents_target_render_surface()) {
      DCHECK(it->render_surface()->draw_opacity() ||
             it->render_surface()->draw_opacity_is_animating());
      need_more_updates |= PaintMasksForRenderSurface(*it, queue, stats_ptr);
    } else if (it.represents_itself()) {
      DCHECK(!it->bounds().IsEmpty());
      it->Update(queue, &occlusion_tracker, stats_ptr);
      need_more_updates |= it->NeedMoreUpdates();
    }

    occlusion_tracker.LeaveLayer(it);
  }

  rendering_stats_instrumentation_->AddStats(stats);

  occlusion_tracker.overdraw_metrics()->RecordMetrics(this);

  return need_more_updates;
}

void LayerTreeHost::ApplyScrollAndScale(const ScrollAndScaleSet& info) {
  if (!root_layer_)
    return;

  Layer* root_scroll_layer = FindFirstScrollableLayer(root_layer_.get());
  gfx::Vector2d root_scroll_delta;

  for (size_t i = 0; i < info.scrolls.size(); ++i) {
    Layer* layer =
        LayerTreeHostCommon::FindLayerInSubtree(root_layer_.get(),
                                                info.scrolls[i].layer_id);
    if (!layer)
      continue;
    if (layer == root_scroll_layer)
      root_scroll_delta += info.scrolls[i].scroll_delta;
    else
      layer->SetScrollOffset(layer->scroll_offset() +
                             info.scrolls[i].scroll_delta);
  }
  if (!root_scroll_delta.IsZero() || info.page_scale_delta != 1.f)
    client_->ApplyScrollAndScale(root_scroll_delta, info.page_scale_delta);
}

void LayerTreeHost::SetImplTransform(const gfx::Transform& transform) {
  impl_transform_ = transform;
}

void LayerTreeHost::StartRateLimiter(WebKit::WebGraphicsContext3D* context3d) {
  if (animating_)
    return;

  DCHECK(context3d);
  RateLimiterMap::iterator it = rate_limiters_.find(context3d);
  if (it != rate_limiters_.end()) {
    it->second->Start();
  } else {
    scoped_refptr<RateLimiter> rate_limiter =
        RateLimiter::Create(context3d, this, proxy_->MainThread());
    rate_limiters_[context3d] = rate_limiter;
    rate_limiter->Start();
  }
}

void LayerTreeHost::StopRateLimiter(WebKit::WebGraphicsContext3D* context3d) {
  RateLimiterMap::iterator it = rate_limiters_.find(context3d);
  if (it != rate_limiters_.end()) {
    it->second->Stop();
    rate_limiters_.erase(it);
  }
}

void LayerTreeHost::RateLimit() {
  // Force a no-op command on the compositor context, so that any ratelimiting
  // commands will wait for the compositing context, and therefore for the
  // SwapBuffers.
  proxy_->ForceSerializeOnSwapBuffers();
}

bool LayerTreeHost::RequestPartialTextureUpdate() {
  if (partial_texture_update_requests_ >= settings_.max_partial_texture_updates)
    return false;

  partial_texture_update_requests_++;
  return true;
}

void LayerTreeHost::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor ==  device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  SetNeedsCommit();
}

void LayerTreeHost::EnableHidingTopControls(bool enable) {
  if (!settings_.calculate_top_controls_position)
    return;

  proxy_->ImplThread()->PostTask(
      base::Bind(&TopControlsManager::EnableHidingTopControls,
                 top_controls_manager_weak_ptr_, enable));
}

bool LayerTreeHost::BlocksPendingCommit() const {
  if (!root_layer_)
    return false;
  return root_layer_->BlocksPendingCommitRecursive();
}

scoped_ptr<base::Value> LayerTreeHost::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->Set("proxy", proxy_->AsValue().release());
  return state.PassAs<base::Value>();
}

void LayerTreeHost::AnimateLayers(base::TimeTicks time) {
  if (!settings_.accelerated_animation_enabled ||
      animation_registrar_->active_animation_controllers().empty())
    return;

  TRACE_EVENT0("cc", "LayerTreeHostImpl::AnimateLayers");

  double monotonic_time = (time - base::TimeTicks()).InSecondsF();

  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter) {
    (*iter).second->Animate(monotonic_time);
    (*iter).second->UpdateState(NULL);
  }
}

void LayerTreeHost::SetAnimationEventsRecursive(
    const AnimationEventsVector& events,
    Layer* layer,
    base::Time wall_clock_time) {
  if (!layer)
    return;

  for (size_t event_index = 0; event_index < events.size(); ++event_index) {
    if (layer->id() == events[event_index].layer_id) {
      switch (events[event_index].type) {
        case AnimationEvent::Started:
          layer->NotifyAnimationStarted(events[event_index],
                                        wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::Finished:
          layer->NotifyAnimationFinished(wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::PropertyUpdate:
          layer->NotifyAnimationPropertyUpdate(events[event_index]);
          break;

        default:
          NOTREACHED();
      }
    }
  }

  for (size_t child_index = 0;
       child_index < layer->children().size();
       ++child_index)
    SetAnimationEventsRecursive(events,
                                layer->children()[child_index].get(),
                                wall_clock_time);
}

skia::RefPtr<SkPicture> LayerTreeHost::CapturePicture() {
  return proxy_->CapturePicture();
}

}  // namespace cc
