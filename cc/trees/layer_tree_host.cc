// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <algorithm>
#include <stack>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/overdraw_metrics.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/render_surface.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/size_conversions.h"

namespace {
static int s_num_layer_tree_instances;
}

namespace cc {

RendererCapabilities::RendererCapabilities()
    : best_texture_format(RGBA_8888),
      using_partial_swap(false),
      using_set_visibility(false),
      using_egl_image(false),
      allow_partial_texture_updates(false),
      using_offscreen_context3d(false),
      max_texture_size(0),
      avoid_pow2_textures(false),
      using_map_image(false),
      using_shared_memory_resources(false),
      using_discard_framebuffer(false) {}

RendererCapabilities::~RendererCapabilities() {}

UIResourceRequest::UIResourceRequest(UIResourceRequestType type,
                                     UIResourceId id)
    : type_(type), id_(id) {}

UIResourceRequest::UIResourceRequest(UIResourceRequestType type,
                                     UIResourceId id,
                                     const UIResourceBitmap& bitmap)
    : type_(type), id_(id), bitmap_(new UIResourceBitmap(bitmap)) {}

UIResourceRequest::UIResourceRequest(const UIResourceRequest& request) {
  (*this) = request;
}

UIResourceRequest& UIResourceRequest::operator=(
    const UIResourceRequest& request) {
  type_ = request.type_;
  id_ = request.id_;
  if (request.bitmap_) {
    bitmap_ = make_scoped_ptr(new UIResourceBitmap(*request.bitmap_.get()));
  } else {
    bitmap_.reset();
  }

  return *this;
}

UIResourceRequest::~UIResourceRequest() {}

bool LayerTreeHost::AnyLayerTreeHostInstanceExists() {
  return s_num_layer_tree_instances > 0;
}

scoped_ptr<LayerTreeHost> LayerTreeHost::Create(
    LayerTreeHostClient* client,
    SharedBitmapManager* manager,
    const LayerTreeSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  scoped_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(client, manager, settings));
  if (!layer_tree_host->Initialize(impl_task_runner))
    return scoped_ptr<LayerTreeHost>();
  return layer_tree_host.Pass();
}

static int s_next_tree_id = 1;

LayerTreeHost::LayerTreeHost(LayerTreeHostClient* client,
                             SharedBitmapManager* manager,
                             const LayerTreeSettings& settings)
    : next_ui_resource_id_(1),
      animating_(false),
      needs_full_tree_sync_(true),
      needs_filter_context_(false),
      client_(client),
      source_frame_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      micro_benchmark_controller_(this),
      output_surface_can_be_initialized_(true),
      output_surface_lost_(true),
      num_failed_recreate_attempts_(0),
      settings_(settings),
      debug_state_(settings.initial_debug_state),
      overdraw_bottom_height_(0.f),
      device_scale_factor_(1.f),
      visible_(true),
      page_scale_factor_(1.f),
      min_page_scale_factor_(1.f),
      max_page_scale_factor_(1.f),
      trigger_idle_updates_(true),
      background_color_(SK_ColorWHITE),
      has_transparent_background_(false),
      partial_texture_update_requests_(0),
      in_paint_layer_contents_(false),
      total_frames_used_for_lcd_text_metrics_(0),
      tree_id_(s_next_tree_id++),
      next_commit_forces_redraw_(false),
      shared_bitmap_manager_(manager) {
  if (settings_.accelerated_animation_enabled)
    animation_registrar_ = AnimationRegistrar::Create();
  s_num_layer_tree_instances++;
  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

bool LayerTreeHost::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  if (impl_task_runner.get())
    return InitializeProxy(ThreadProxy::Create(this, impl_task_runner));
  else
    return InitializeProxy(SingleThreadProxy::Create(this));
}

bool LayerTreeHost::InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing) {
  return InitializeProxy(proxy_for_testing.Pass());
}

bool LayerTreeHost::InitializeProxy(scoped_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");

  scoped_ptr<OutputSurface> output_surface(CreateOutputSurface());
  if (!output_surface)
    return false;

  proxy_ = proxy.Pass();
  proxy_->Start(output_surface.Pass());
  return true;
}

LayerTreeHost::~LayerTreeHost() {
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  overhang_ui_resource_.reset();

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);

  if (proxy_) {
    DCHECK(proxy_->IsMainThread());
    proxy_->Stop();
  }

  s_num_layer_tree_instances--;
  RateLimiterMap::iterator it = rate_limiters_.begin();
  if (it != rate_limiters_.end())
    it->second->Stop();

  if (root_layer_.get()) {
    // The layer tree must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the registrar
    // will outlive them, and we must make good.
    root_layer_ = NULL;
  }
}

void LayerTreeHost::SetLayerTreeHostClientReady() {
  proxy_->SetLayerTreeHostClientReady();
}

static void LayerTreeHostOnOutputSurfaceCreatedCallback(Layer* layer) {
  layer->OnOutputSurfaceCreated();
}

LayerTreeHost::CreateResult
LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted(bool success) {
  TRACE_EVENT1("cc",
               "LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted",
               "success",
               success);

  DCHECK(output_surface_lost_);
  if (success) {
    output_surface_lost_ = false;

    if (!contents_texture_manager_ && !settings_.impl_side_painting) {
      contents_texture_manager_ =
          PrioritizedResourceManager::Create(proxy_.get());
      surface_memory_placeholder_ =
          contents_texture_manager_->CreateTexture(gfx::Size(), RGBA_8888);
    }

    if (root_layer()) {
      LayerTreeHostCommon::CallFunctionForSubtree(
          root_layer(),
          base::Bind(&LayerTreeHostOnOutputSurfaceCreatedCallback));
    }

    client_->DidInitializeOutputSurface(true);
    return CreateSucceeded;
  }

  // Failure path.

  client_->DidFailToInitializeOutputSurface();

  // Tolerate a certain number of recreation failures to work around races
  // in the output-surface-lost machinery.
  ++num_failed_recreate_attempts_;
  if (num_failed_recreate_attempts_ >= 5) {
    // We have tried too many times to recreate the output surface. Tell the
    // host to fall back to software rendering.
    output_surface_can_be_initialized_ = false;
    client_->DidInitializeOutputSurface(false);
    return CreateFailedAndGaveUp;
  }

  return CreateFailedButTryAgain;
}

void LayerTreeHost::DeleteContentsTexturesOnImplThread(
    ResourceProvider* resource_provider) {
  DCHECK(proxy_->IsImplThread());
  if (contents_texture_manager_)
    contents_texture_manager_->ClearAllMemory(resource_provider);
}

void LayerTreeHost::AcquireLayerTextures() {
  DCHECK(proxy_->IsMainThread());
  proxy_->AcquireLayerTextures();
}

void LayerTreeHost::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void LayerTreeHost::UpdateClientAnimations(base::TimeTicks frame_begin_time) {
  animating_ = true;
  client_->Animate((frame_begin_time - base::TimeTicks()).InSecondsF());
  animating_ = false;
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
  bool new_impl_tree_has_no_evicted_resources = false;
  if (contents_texture_manager_) {
    new_impl_tree_has_no_evicted_resources =
        !contents_texture_manager_->LinkedEvictedBackingsExist();

    // If the memory limit has been increased since this now-finishing
    // commit began, and the extra now-available memory would have been used,
    // then request another commit.
    if (contents_texture_manager_->MaxMemoryLimitBytes() <
            host_impl->memory_allocation_limit_bytes() &&
        contents_texture_manager_->MaxMemoryLimitBytes() <
            contents_texture_manager_->MaxMemoryNeededBytes()) {
      host_impl->SetNeedsCommit();
    }

    host_impl->set_max_memory_needed_bytes(
        contents_texture_manager_->MaxMemoryNeededBytes());

    contents_texture_manager_->UpdateBackingsState(
        host_impl->resource_provider());
  }

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
    if (next_commit_forces_redraw_)
      sync_tree->ForceRedrawNextActivation();
  } else {
    if (next_commit_forces_redraw_)
      host_impl->SetFullRootLayerDamage();
    contents_texture_manager_->ReduceMemory(host_impl->resource_provider());
    sync_tree = host_impl->active_tree();
  }

  next_commit_forces_redraw_ = false;

  sync_tree->set_source_frame_number(source_frame_number());

  if (needs_full_tree_sync_)
    sync_tree->SetRootLayer(TreeSynchronizer::SynchronizeTrees(
        root_layer(), sync_tree->DetachLayerTree(), sync_tree));
  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");
    TreeSynchronizer::PushProperties(root_layer(), sync_tree->root_layer());
  }

  sync_tree->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = LayerTreeHostCommon::FindLayerInSubtree(
        sync_tree->root_layer(), hud_layer_->id());
    sync_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    sync_tree->set_hud_layer(NULL);
  }

  sync_tree->set_background_color(background_color_);
  sync_tree->set_has_transparent_background(has_transparent_background_);

  sync_tree->FindRootScrollLayer();

  // TODO(wjmaclean) For now, not all LTH clients will register viewports, so
  // only set them when available..
  if (page_scale_layer_) {
    DCHECK(inner_viewport_scroll_layer_);
    sync_tree->SetViewportLayersFromIds(
        page_scale_layer_->id(),
        inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                     : Layer::INVALID_ID);
  } else {
    sync_tree->ClearViewportLayers();
  }

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
  sync_tree->SetLatencyInfo(latency_info_);
  latency_info_.Clear();

  host_impl->SetViewportSize(device_viewport_size_);
  host_impl->SetOverdrawBottomHeight(overdraw_bottom_height_);
  host_impl->SetDeviceScaleFactor(device_scale_factor_);
  host_impl->SetDebugState(debug_state_);
  if (pending_page_scale_animation_) {
    host_impl->StartPageScaleAnimation(
        pending_page_scale_animation_->target_offset,
        pending_page_scale_animation_->use_anchor,
        pending_page_scale_animation_->scale,
        pending_page_scale_animation_->duration);
    pending_page_scale_animation_.reset();
  }

  if (!ui_resource_request_queue_.empty()) {
    sync_tree->set_ui_resource_request_queue(ui_resource_request_queue_);
    ui_resource_request_queue_.clear();
    // Process any ui resource requests in the queue.  For impl-side-painting,
    // the queue is processed in LayerTreeHostImpl::ActivatePendingTree.
    if (!settings_.impl_side_painting)
      sync_tree->ProcessUIResourceRequestQueue();
  }
  if (overhang_ui_resource_) {
    host_impl->SetOverhangUIResource(
        overhang_ui_resource_->id(),
        GetUIResourceSize(overhang_ui_resource_->id()));
  }

  DCHECK(!sync_tree->ViewportSizeInvalid());

  if (new_impl_tree_has_no_evicted_resources) {
    if (sync_tree->ContentsTexturesPurged())
      sync_tree->ResetContentsTexturesPurged();
  }

  if (!settings_.impl_side_painting) {
    // If we're not in impl-side painting, the tree is immediately
    // considered active.
    sync_tree->DidBecomeActive();
  }

  source_frame_number_++;
}

void LayerTreeHost::WillCommit() {
  client_->WillCommit();
}

void LayerTreeHost::UpdateHudLayer() {
  if (debug_state_.ShowHudInfo()) {
    if (!hud_layer_.get())
      hud_layer_ = HeadsUpDisplayLayer::Create();

    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = NULL;
  }
}

void LayerTreeHost::CommitComplete() {
  client_->DidCommit();
}

scoped_ptr<OutputSurface> LayerTreeHost::CreateOutputSurface() {
  return client_->CreateOutputSurface(num_failed_recreate_attempts_ >= 4);
}

scoped_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(proxy_->IsImplThread());
  scoped_ptr<LayerTreeHostImpl> host_impl =
      LayerTreeHostImpl::Create(settings_,
                                client,
                                proxy_.get(),
                                rendering_stats_instrumentation_.get(),
                                shared_bitmap_manager_);
  shared_bitmap_manager_ = NULL;
  if (settings_.calculate_top_controls_position &&
      host_impl->top_controls_manager()) {
    top_controls_manager_weak_ptr_ =
        host_impl->top_controls_manager()->AsWeakPtr();
  }
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl.Pass();
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(proxy_->IsMainThread());

  if (output_surface_lost_)
    return;

  num_failed_recreate_attempts_ = 0;
  output_surface_lost_ = true;
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
  proxy_->FinishAllRendering();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::DidDeferCommit() {}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  std::stack<Layer*> layer_stack;
  layer_stack.push(root_layer());
  while (!layer_stack.empty()) {
    Layer* current_layer = layer_stack.top();
    layer_stack.pop();
    current_layer->SetNeedsDisplay();
    for (unsigned int i = 0; i < current_layer->children().size(); i++) {
      layer_stack.push(current_layer->child_at(i));
    }
  }
}

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

void LayerTreeHost::SetNeedsUpdateLayers() { proxy_->SetNeedsUpdateLayers(); }

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
  SetNeedsRedrawRect(gfx::Rect(device_viewport_size_));
}

void LayerTreeHost::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
  if (!proxy_->HasImplThread())
    client_->ScheduleComposite();
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

bool LayerTreeHost::BeginMainFrameRequested() const {
  return proxy_->BeginMainFrameRequested();
}


void LayerTreeHost::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHost::SetNextCommitForcesRedraw() {
  next_commit_forces_redraw_ = true;
}

void LayerTreeHost::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                                       base::Time wall_clock_time) {
  DCHECK(proxy_->IsMainThread());
  for (size_t event_index = 0; event_index < events->size(); ++event_index) {
    int event_layer_id = (*events)[event_index].layer_id;

    // Use the map of all controllers, not just active ones, since non-active
    // controllers may still receive events for impl-only animations.
    const AnimationRegistrar::AnimationControllerMap& animation_controllers =
        animation_registrar_->all_animation_controllers();
    AnimationRegistrar::AnimationControllerMap::const_iterator iter =
        animation_controllers.find(event_layer_id);
    if (iter != animation_controllers.end()) {
      switch ((*events)[event_index].type) {
        case AnimationEvent::Started:
          (*iter).second->NotifyAnimationStarted((*events)[event_index],
                                                 wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::Finished:
          (*iter).second->NotifyAnimationFinished((*events)[event_index],
                                                  wall_clock_time.ToDoubleT());
          break;

        case AnimationEvent::PropertyUpdate:
          (*iter).second->NotifyAnimationPropertyUpdate((*events)[event_index]);
          break;
      }
    }
  }
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(this);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

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

void LayerTreeHost::SetViewportSize(gfx::Size device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;

  device_viewport_size_ = device_viewport_size;

  SetNeedsCommit();
}

void LayerTreeHost::SetOverdrawBottomHeight(float overdraw_bottom_height) {
  if (overdraw_bottom_height_ == overdraw_bottom_height)
    return;

  overdraw_bottom_height_ = overdraw_bottom_height;
  SetNeedsCommit();
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(float page_scale_delta) {
  DCHECK(CommitRequested());
  page_scale_factor_ *= page_scale_delta;
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

void LayerTreeHost::SetOverhangBitmap(const SkBitmap& bitmap) {
  DCHECK(bitmap.width() && bitmap.height());
  DCHECK_EQ(bitmap.bytesPerPixel(), 4);

  SkBitmap bitmap_copy;
  if (bitmap.isImmutable()) {
    bitmap_copy = bitmap;
  } else {
    bitmap.copyTo(&bitmap_copy, bitmap.config());
    bitmap_copy.setImmutable();
  }

  UIResourceBitmap overhang_bitmap(bitmap_copy);
  overhang_bitmap.SetWrapMode(UIResourceBitmap::REPEAT);
  overhang_ui_resource_ = ScopedUIResource::Create(this, overhang_bitmap);
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  if (!visible)
    ReduceMemoryUsage();
  proxy_->SetVisible(visible);
}

void LayerTreeHost::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  latency_info_.MergeWith(latency_info);
}

void LayerTreeHost::StartPageScaleAnimation(gfx::Vector2d target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(new PendingPageScaleAnimation);
  pending_page_scale_animation_->target_offset = target_offset;
  pending_page_scale_animation_->use_anchor = use_anchor;
  pending_page_scale_animation_->scale = scale;
  pending_page_scale_animation_->duration = duration;

  SetNeedsCommit();
}

void LayerTreeHost::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
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

bool LayerTreeHost::InitializeOutputSurfaceIfNeeded() {
  if (!output_surface_can_be_initialized_)
    return false;

  if (output_surface_lost_)
    proxy_->CreateAndInitializeOutputSurface();
  return !output_surface_lost_;
}

bool LayerTreeHost::UpdateLayers(ResourceUpdateQueue* queue) {
  DCHECK(!output_surface_lost_);

  if (!root_layer())
    return false;

  DCHECK(!root_layer()->parent());

  bool result = UpdateLayers(root_layer(), queue);

  micro_benchmark_controller_.DidUpdateLayers();

  return result;
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

void LayerTreeHost::CalculateLCDTextMetricsCallback(Layer* layer) {
  if (!layer->SupportsLCDText())
    return;

  lcd_text_metrics_.total_num_cc_layers++;
  if (layer->draw_properties().can_use_lcd_text) {
    lcd_text_metrics_.total_num_cc_layers_can_use_lcd_text++;
    if (layer->contents_opaque())
      lcd_text_metrics_.total_num_cc_layers_will_use_lcd_text++;
  }
}

bool LayerTreeHost::UsingSharedMemoryResources() {
  return GetRendererCapabilities().using_shared_memory_resources;
}

bool LayerTreeHost::UpdateLayers(Layer* root_layer,
                                 ResourceUpdateQueue* queue) {
  TRACE_EVENT1("cc", "LayerTreeHost::UpdateLayers",
               "source_frame_number", source_frame_number());

  RenderSurfaceLayerList update_list;
  {
    UpdateHudLayer();

    Layer* root_scroll = FindFirstScrollableLayer(root_layer);
    Layer* page_scale_layer = page_scale_layer_;
    if (!page_scale_layer && root_scroll)
      page_scale_layer = root_scroll->parent();

    if (hud_layer_) {
      hud_layer_->PrepareForCalculateDrawProperties(
          device_viewport_size(), device_scale_factor_);
    }

    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::CalcDrawProps");
    bool can_render_to_separate_surface = true;
    LayerTreeHostCommon::CalcDrawPropsMainInputs inputs(
        root_layer,
        device_viewport_size(),
        gfx::Transform(),
        device_scale_factor_,
        page_scale_factor_,
        page_scale_layer,
        GetRendererCapabilities().max_texture_size,
        settings_.can_use_lcd_text,
        can_render_to_separate_surface,
        settings_.layer_transforms_should_scale_layer_contents,
        &update_list);
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    if (total_frames_used_for_lcd_text_metrics_ <=
        kTotalFramesToUseForLCDTextMetrics) {
      LayerTreeHostCommon::CallFunctionForSubtree(
          root_layer,
          base::Bind(&LayerTreeHost::CalculateLCDTextMetricsCallback,
                     base::Unretained(this)));
      total_frames_used_for_lcd_text_metrics_++;
    }

    if (total_frames_used_for_lcd_text_metrics_ ==
        kTotalFramesToUseForLCDTextMetrics) {
      total_frames_used_for_lcd_text_metrics_++;

      UMA_HISTOGRAM_PERCENTAGE(
          "Renderer4.LCDText.PercentageOfCandidateLayers",
          lcd_text_metrics_.total_num_cc_layers_can_use_lcd_text * 100.0 /
          lcd_text_metrics_.total_num_cc_layers);
      UMA_HISTOGRAM_PERCENTAGE(
          "Renderer4.LCDText.PercentageOfAALayers",
          lcd_text_metrics_.total_num_cc_layers_will_use_lcd_text * 100.0 /
          lcd_text_metrics_.total_num_cc_layers_can_use_lcd_text);
    }
  }

  // Reset partial texture update requests.
  partial_texture_update_requests_ = 0;

  bool did_paint_content = false;
  bool need_more_updates = false;
  PaintLayerContents(
      update_list, queue, &did_paint_content, &need_more_updates);
  if (trigger_idle_updates_ && need_more_updates) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::posting prepaint task");
    prepaint_callback_.Reset(base::Bind(&LayerTreeHost::TriggerPrepaint,
                                        base::Unretained(this)));
    static base::TimeDelta prepaint_delay =
        base::TimeDelta::FromMilliseconds(100);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, prepaint_callback_.callback(), prepaint_delay);
  }

  return did_paint_content;
}

void LayerTreeHost::TriggerPrepaint() {
  prepaint_callback_.Cancel();
  TRACE_EVENT0("cc", "LayerTreeHost::TriggerPrepaint");
  SetNeedsCommit();
}

static void LayerTreeHostReduceMemoryCallback(Layer* layer) {
  layer->ReduceMemoryUsage();
}

void LayerTreeHost::ReduceMemoryUsage() {
  if (!root_layer())
    return;

  LayerTreeHostCommon::CallFunctionForSubtree(
      root_layer(),
      base::Bind(&LayerTreeHostReduceMemoryCallback));
}

void LayerTreeHost::SetPrioritiesForSurfaces(size_t surface_memory_bytes) {
  DCHECK(surface_memory_placeholder_);

  // Surfaces have a place holder for their memory since they are managed
  // independantly but should still be tracked and reduce other memory usage.
  surface_memory_placeholder_->SetTextureManager(
      contents_texture_manager_.get());
  surface_memory_placeholder_->set_request_priority(
      PriorityCalculator::RenderSurfacePriority());
  surface_memory_placeholder_->SetToSelfManagedMemoryPlaceholder(
      surface_memory_bytes);
}

void LayerTreeHost::SetPrioritiesForLayers(
    const RenderSurfaceLayerList& update_list) {
  typedef LayerIterator<Layer,
                        RenderSurfaceLayerList,
                        RenderSurface,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;

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
    const RenderSurfaceLayerList& render_surface_layer_list,
    OverdrawMetrics* metrics) {
  if (!contents_texture_manager_)
    return;

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
    const RenderSurfaceLayerList& update_list) {
  size_t readback_bytes = 0;
  size_t max_background_texture_bytes = 0;
  size_t contents_texture_bytes = 0;

  // Start iteration at 1 to skip the root surface as it does not have a texture
  // cost.
  for (size_t i = 1; i < update_list.size(); ++i) {
    Layer* render_surface_layer = update_list.at(i);
    RenderSurface* render_surface = render_surface_layer->render_surface();

    size_t bytes =
        Resource::MemorySizeBytes(render_surface->content_rect().size(),
                                  RGBA_8888);
    contents_texture_bytes += bytes;

    if (render_surface_layer->background_filters().IsEmpty())
      continue;

    if (bytes > max_background_texture_bytes)
      max_background_texture_bytes = bytes;
    if (!readback_bytes) {
      readback_bytes = Resource::MemorySizeBytes(device_viewport_size_,
                                                 RGBA_8888);
    }
  }
  return readback_bytes + max_background_texture_bytes + contents_texture_bytes;
}

void LayerTreeHost::PaintMasksForRenderSurface(Layer* render_surface_layer,
                                               ResourceUpdateQueue* queue,
                                               bool* did_paint_content,
                                               bool* need_more_updates) {
  // Note: Masks and replicas only exist for layers that own render surfaces. If
  // we reach this point in code, we already know that at least something will
  // be drawn into this render surface, so the mask and replica should be
  // painted.

  Layer* mask_layer = render_surface_layer->mask_layer();
  if (mask_layer) {
    devtools_instrumentation::ScopedLayerTreeTask
        update_layer(devtools_instrumentation::kUpdateLayer,
                     mask_layer->id(),
                     id());
    *did_paint_content |= mask_layer->Update(queue, NULL);
    *need_more_updates |= mask_layer->NeedMoreUpdates();
  }

  Layer* replica_mask_layer =
      render_surface_layer->replica_layer() ?
      render_surface_layer->replica_layer()->mask_layer() : NULL;
  if (replica_mask_layer) {
    devtools_instrumentation::ScopedLayerTreeTask
        update_layer(devtools_instrumentation::kUpdateLayer,
                     replica_mask_layer->id(),
                     id());
    *did_paint_content |= replica_mask_layer->Update(queue, NULL);
    *need_more_updates |= replica_mask_layer->NeedMoreUpdates();
  }
}

void LayerTreeHost::PaintLayerContents(
    const RenderSurfaceLayerList& render_surface_layer_list,
    ResourceUpdateQueue* queue,
    bool* did_paint_content,
    bool* need_more_updates) {
  // Use FrontToBack to allow for testing occlusion and performing culling
  // during the tree walk.
  typedef LayerIterator<Layer,
                        RenderSurfaceLayerList,
                        RenderSurface,
                        LayerIteratorActions::FrontToBack> LayerIteratorType;

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

  in_paint_layer_contents_ = true;

  LayerIteratorType end = LayerIteratorType::End(&render_surface_layer_list);
  for (LayerIteratorType it =
           LayerIteratorType::Begin(&render_surface_layer_list);
       it != end;
       ++it) {
    bool prevent_occlusion = it.target_render_surface_layer()->HasCopyRequest();
    occlusion_tracker.EnterLayer(it, prevent_occlusion);

    if (it.represents_target_render_surface()) {
      PaintMasksForRenderSurface(
          *it, queue, did_paint_content, need_more_updates);
    } else if (it.represents_itself() && !it->draw_properties().skip_drawing) {
      devtools_instrumentation::ScopedLayerTreeTask
          update_layer(devtools_instrumentation::kUpdateLayer, it->id(), id());
      DCHECK(!it->paint_properties().bounds.IsEmpty());
      *did_paint_content |= it->Update(queue, &occlusion_tracker);
      *need_more_updates |= it->NeedMoreUpdates();
    }

    occlusion_tracker.LeaveLayer(it);
  }

  in_paint_layer_contents_ = false;

  occlusion_tracker.overdraw_metrics()->RecordMetrics(this);
}

void LayerTreeHost::ApplyScrollAndScale(const ScrollAndScaleSet& info) {
  if (!root_layer_.get())
    return;

  gfx::Vector2d root_scroll_delta;
  Layer* root_scroll_layer = FindFirstScrollableLayer(root_layer_.get());

  for (size_t i = 0; i < info.scrolls.size(); ++i) {
    Layer* layer =
        LayerTreeHostCommon::FindLayerInSubtree(root_layer_.get(),
                                                info.scrolls[i].layer_id);
    if (!layer)
      continue;
    if (layer == root_scroll_layer) {
      root_scroll_delta += info.scrolls[i].scroll_delta;
    } else {
      layer->SetScrollOffsetFromImplSide(layer->scroll_offset() +
                                         info.scrolls[i].scroll_delta);
    }
  }

  if (!root_scroll_delta.IsZero() || info.page_scale_delta != 1.f) {
    // SetScrollOffsetFromImplSide above could have destroyed the tree,
    // so re-get this layer before doing anything to it.
    root_scroll_layer = FindFirstScrollableLayer(root_layer_.get());

    // Preemptively apply the scroll offset and scale delta here before sending
    // it to the client.  If the client comes back and sets it to the same
    // value, then the layer can early out without needing a full commit.
    if (root_scroll_layer) {
      root_scroll_layer->SetScrollOffsetFromImplSide(
          root_scroll_layer->scroll_offset() + root_scroll_delta);
    }
    ApplyPageScaleDeltaFromImplSide(info.page_scale_delta);
    client_->ApplyScrollAndScale(root_scroll_delta, info.page_scale_delta);
  }
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
        RateLimiter::Create(context3d, this, proxy_->MainThreadTaskRunner());
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

bool LayerTreeHost::AlwaysUsePartialTextureUpdates() {
  if (!proxy_->GetRendererCapabilities().allow_partial_texture_updates)
    return false;
  return !proxy_->HasImplThread();
}

size_t LayerTreeHost::MaxPartialTextureUpdates() const {
  size_t max_partial_texture_updates = 0;
  if (proxy_->GetRendererCapabilities().allow_partial_texture_updates &&
      !settings_.impl_side_painting) {
    max_partial_texture_updates =
        std::min(settings_.max_partial_texture_updates,
                 proxy_->MaxPartialTextureUpdates());
  }
  return max_partial_texture_updates;
}

bool LayerTreeHost::RequestPartialTextureUpdate() {
  if (partial_texture_update_requests_ >= MaxPartialTextureUpdates())
    return false;

  partial_texture_update_requests_++;
  return true;
}

void LayerTreeHost::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  SetNeedsCommit();
}

void LayerTreeHost::UpdateTopControlsState(TopControlsState constraints,
                                           TopControlsState current,
                                           bool animate) {
  if (!settings_.calculate_top_controls_position)
    return;

  // Top controls are only used in threaded mode.
  proxy_->ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&TopControlsManager::UpdateTopControlsState,
                 top_controls_manager_weak_ptr_,
                 constraints,
                 current,
                 animate));
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

  TRACE_EVENT0("cc", "LayerTreeHost::AnimateLayers");

  double monotonic_time = (time - base::TimeTicks()).InSecondsF();

  AnimationRegistrar::AnimationControllerMap copy =
      animation_registrar_->active_animation_controllers();
  for (AnimationRegistrar::AnimationControllerMap::iterator iter = copy.begin();
       iter != copy.end();
       ++iter) {
    (*iter).second->Animate(monotonic_time);
    bool start_ready_animations = true;
    (*iter).second->UpdateState(start_ready_animations, NULL);
  }
}

UIResourceId LayerTreeHost::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(ui_resource_client_map_.find(next_id) ==
         ui_resource_client_map_.end());

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::UIResourceCreate,
                            next_id,
                            client->GetBitmap(next_id, resource_lost));
  ui_resource_request_queue_.push_back(request);

  UIResourceClientData data;
  data.client = client;
  data.size = request.GetBitmap().GetSize();

  ui_resource_client_map_[request.GetId()] = data;
  return request.GetId();
}

// Deletes a UI resource.  May safely be called more than once.
void LayerTreeHost::DeleteUIResource(UIResourceId uid) {
  UIResourceClientMap::iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return;

  UIResourceRequest request(UIResourceRequest::UIResourceDelete, uid);
  ui_resource_request_queue_.push_back(request);
  ui_resource_client_map_.erase(iter);
}

void LayerTreeHost::RecreateUIResources() {
  for (UIResourceClientMap::iterator iter = ui_resource_client_map_.begin();
       iter != ui_resource_client_map_.end();
       ++iter) {
    UIResourceId uid = iter->first;
    const UIResourceClientData& data = iter->second;
    bool resource_lost = true;
    UIResourceRequest request(UIResourceRequest::UIResourceCreate,
                              uid,
                              data.client->GetBitmap(uid, resource_lost));
    ui_resource_request_queue_.push_back(request);
  }
}

// Returns the size of a resource given its id.
gfx::Size LayerTreeHost::GetUIResourceSize(UIResourceId uid) const {
  UIResourceClientMap::const_iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return gfx::Size();

  const UIResourceClientData& data = iter->second;
  return data.size;
}

void LayerTreeHost::RegisterViewportLayers(
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  page_scale_layer_ = page_scale_layer;
  inner_viewport_scroll_layer_ = inner_viewport_scroll_layer;
  outer_viewport_scroll_layer_ = outer_viewport_scroll_layer;
}

bool LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  return micro_benchmark_controller_.ScheduleRun(
      benchmark_name, value.Pass(), callback);
}

}  // namespace cc
