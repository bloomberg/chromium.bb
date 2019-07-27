// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/tree_synchronizer.h"
#include "cc/trees/ukm_manager.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/presentation_feedback.h"

namespace {
static base::AtomicSequenceNumber s_layer_tree_host_sequence_number;
static base::AtomicSequenceNumber s_image_decode_sequence_number;
}

namespace cc {
namespace {

bool AreEmbedTokensEqual(const viz::LocalSurfaceId& lsi1,
                         const viz::LocalSurfaceId& lsi2) {
  return lsi1.embed_token() == lsi2.embed_token();
}

bool AreParentSequencesEqual(const viz::LocalSurfaceId& lsi1,
                             const viz::LocalSurfaceId& lsi2) {
  return lsi1.parent_sequence_number() == lsi2.parent_sequence_number();
}

}  // namespace

LayerTreeHost::InitParams::InitParams() = default;
LayerTreeHost::InitParams::~InitParams() = default;
LayerTreeHost::InitParams::InitParams(InitParams&&) = default;
LayerTreeHost::InitParams& LayerTreeHost::InitParams::operator=(InitParams&&) =
    default;

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams params) {
  DCHECK(params.settings);
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      params.main_task_runner;
  DCHECK(main_task_runner);
  DCHECK(impl_task_runner);
  auto layer_tree_host = base::WrapUnique(
      new LayerTreeHost(std::move(params), CompositorMode::THREADED));
  layer_tree_host->InitializeThreaded(std::move(main_task_runner),
                                      std::move(impl_task_runner));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams params) {
  DCHECK(params.settings);
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      params.main_task_runner;
  auto layer_tree_host = base::WrapUnique(
      new LayerTreeHost(std::move(params), CompositorMode::SINGLE_THREADED));
  layer_tree_host->InitializeSingleThreaded(single_thread_client,
                                            std::move(main_task_runner));
  return layer_tree_host;
}

LayerTreeHost::LayerTreeHost(InitParams params, CompositorMode mode)
    : micro_benchmark_controller_(this),
      image_worker_task_runner_(std::move(params.image_worker_task_runner)),
      ukm_recorder_factory_(std::move(params.ukm_recorder_factory)),
      compositor_mode_(mode),
      ui_resource_manager_(std::make_unique<UIResourceManager>()),
      client_(params.client),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      settings_(*params.settings),
      debug_state_(settings_.initial_debug_state),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      task_graph_runner_(params.task_graph_runner),
      event_listener_properties_(),
      mutator_host_(params.mutator_host) {
  DCHECK(task_graph_runner_);
  DCHECK(!settings_.enable_checker_imaging || image_worker_task_runner_);

  mutator_host_->SetMutatorHostClient(this);

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

void LayerTreeHost::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
  std::unique_ptr<ProxyMain> proxy_main =
      std::make_unique<ProxyMain>(this, task_runner_provider_.get());
  InitializeProxy(std::move(proxy_main));
}

void LayerTreeHost::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);
  InitializeProxy(SingleThreadProxy::Create(this, single_thread_client,
                                            task_runner_provider_.get()));
}

void LayerTreeHost::InitializeForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider,
    std::unique_ptr<Proxy> proxy_for_testing) {
  task_runner_provider_ = std::move(task_runner_provider);
  InitializeProxy(std::move(proxy_for_testing));
}

void LayerTreeHost::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  DCHECK(!task_runner_provider_);
  task_runner_provider_ = std::move(task_runner_provider);
}

void LayerTreeHost::SetUIResourceManagerForTesting(
    std::unique_ptr<UIResourceManager> ui_resource_manager) {
  ui_resource_manager_ = std::move(ui_resource_manager);
}

void LayerTreeHost::InitializeProxy(std::unique_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");
  DCHECK(task_runner_provider_);

  proxy_ = std::move(proxy);
  proxy_->Start();

  UpdateDeferMainFrameUpdateInternal();

  mutator_host_->SetSupportsScrollAnimations(proxy_->SupportsImplScrolling());
}

LayerTreeHost::~LayerTreeHost() {
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/895883
  CHECK(!inside_main_frame_);
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  // Clear any references into the LayerTreeHost.
  mutator_host_->SetMutatorHostClient(nullptr);

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(ViewportLayers());

  if (root_layer_) {
    root_layer_->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    root_layer_ = nullptr;
  }

  // Fail any pending image decodes.
  for (auto& pair : pending_image_decodes_)
    std::move(pair.second).Run(false);

  if (proxy_) {
    DCHECK(task_runner_provider_->IsMainThread());
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }
}

int LayerTreeHost::GetId() const {
  return id_;
}

int LayerTreeHost::SourceFrameNumber() const {
  return source_frame_number_;
}

UIResourceManager* LayerTreeHost::GetUIResourceManager() const {
  return ui_resource_manager_.get();
}

TaskRunnerProvider* LayerTreeHost::GetTaskRunnerProvider() const {
  return task_runner_provider_.get();
}

SwapPromiseManager* LayerTreeHost::GetSwapPromiseManager() {
  return &swap_promise_manager_;
}

const LayerTreeSettings& LayerTreeHost::GetSettings() const {
  return settings_;
}

void LayerTreeHost::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));

  // Request a main frame if one is not already in progress. This might either
  // A) request a commit ahead of time or B) request a commit which is not
  // needed because there are not pending updates. If B) then the frame will
  // be aborted early and the swap promises will be broken (see
  // EarlyOut_NoUpdates).
  if (!inside_main_frame_)
    SetNeedsAnimate();
}

void LayerTreeHost::WillBeginMainFrame() {
  inside_main_frame_ = true;
  devtools_instrumentation::WillBeginMainThreadFrame(GetId(),
                                                     SourceFrameNumber());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  inside_main_frame_ = false;
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {
  client_->BeginMainFrameNotExpectedUntil(time);
}

void LayerTreeHost::BeginMainFrame(const viz::BeginFrameArgs& args) {
  client_->BeginMainFrame(args);
}

const LayerTreeDebugState& LayerTreeHost::GetDebugState() const {
  return debug_state_;
}

void LayerTreeHost::RequestMainFrameUpdate() {
  client_->UpdateLayerTreeHost();
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(
    LayerTreeHostImpl* host_impl) {
  DCHECK(task_runner_provider_->IsImplThread());

  TRACE_EVENT0("cc,benchmark", "LayerTreeHost::FinishCommitOnImplThread");

  LayerTreeImpl* sync_tree = host_impl->sync_tree();
  sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kBeginningSync);

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }
  if (next_commit_forces_recalculate_raster_scales_) {
    sync_tree->ForceRecalculateRasterScales();
    next_commit_forces_recalculate_raster_scales_ = false;
  }

  sync_tree->set_source_frame_number(SourceFrameNumber());
  if (!pending_presentation_time_callbacks_.empty()) {
    sync_tree->AddPresentationCallbacks(
        std::move(pending_presentation_time_callbacks_));
    pending_presentation_time_callbacks_.clear();
  }

  if (needs_full_tree_sync_)
    TreeSynchronizer::SynchronizeTrees(root_layer(), sync_tree);

  if (clear_caches_on_next_commit_) {
    clear_caches_on_next_commit_ = false;
    proxy_->ClearHistory();
    host_impl->ClearCaches();
  }

  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");

    PushPropertyTreesTo(sync_tree);
    sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kSyncedPropertyTrees);

    PushSurfaceRangesTo(sync_tree);
    TreeSynchronizer::PushLayerProperties(this, sync_tree);
    sync_tree->lifecycle().AdvanceTo(
        LayerTreeLifecycle::kSyncedLayerProperties);

    PushLayerTreePropertiesTo(sync_tree);
    PushLayerTreeHostPropertiesTo(host_impl);

    sync_tree->PassSwapPromises(swap_promise_manager_.TakeSwapPromises());

    sync_tree->set_ui_resource_request_queue(
        ui_resource_manager_->TakeUIResourcesRequests());

    // This must happen after synchronizing property trees and after pushing
    // properties, which updates the clobber_active_value flag.
    // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
    sync_tree->property_trees()->scroll_tree.PushScrollUpdatesFromMainThread(
        property_trees(), sync_tree);

    // This must happen after synchronizing property trees and after push
    // properties, which updates property tree indices, but before animation
    // host pushes properties as animation host push properties can change
    // KeyframeModel::InEffect and we want the old InEffect value for updating
    // property tree scrolling and animation.
    // TODO(pdr): Enforce this comment with DCHECKS and a lifecycle state.
    sync_tree->UpdatePropertyTreeAnimationFromMainThread();

    TRACE_EVENT0("cc", "LayerTreeHost::AnimationHost::PushProperties");
    DCHECK(host_impl->mutator_host());
    mutator_host_->PushPropertiesTo(host_impl->mutator_host());

    // Updating elements affects whether animations are in effect based on their
    // properties so run after pushing updated animation properties.
    host_impl->UpdateElements(ElementListType::PENDING);

    sync_tree->lifecycle().AdvanceTo(LayerTreeLifecycle::kNotSyncing);
  }

  // Transfer image decode requests to the impl thread.
  for (auto& request : queued_image_decodes_) {
    int next_id = s_image_decode_sequence_number.GetNext();
    pending_image_decodes_[next_id] = std::move(request.second);
    host_impl->QueueImageDecode(next_id, std::move(request.first));
  }
  queued_image_decodes_.clear();

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
  property_trees_.ResetAllChangeTracking();

  // Dump property trees and layers if run with:
  //   --vmodule=layer_tree_host=3
  if (VLOG_IS_ON(3)) {
    const char* client_name = GetClientNameForMetrics();
    if (!client_name)
      client_name = "<unknown client>";
    VLOG(3) << "After finishing (" << client_name
            << ") commit on impl, the sync tree:"
            << "\nproperty_trees:\n"
            << sync_tree->property_trees()->ToString() << "\n"
            << "cc::LayerImpls:\n"
            << sync_tree->LayerListAsJson();
  }
}

void LayerTreeHost::ImageDecodesFinished(
    const std::vector<std::pair<int, bool>>& results) {
  // Issue stored callbacks and remove them from the pending list.
  for (const auto& pair : results) {
    auto it = pending_image_decodes_.find(pair.first);
    DCHECK(it != pending_image_decodes_.end());
    std::move(it->second).Run(pair.second);
    pending_image_decodes_.erase(it);
  }
}

void LayerTreeHost::PushPropertyTreesTo(LayerTreeImpl* tree_impl) {
  bool property_trees_changed_on_active_tree =
      tree_impl->IsActiveTree() && tree_impl->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (root_layer_ && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        tree_impl->property_trees()->sequence_number)
      tree_impl->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      tree_impl->MoveChangeTrackingToLayers();
  }

  tree_impl->SetPropertyTrees(&property_trees_);
}

void LayerTreeHost::WillCommit() {
  swap_promise_manager_.WillCommit();
  client_->WillCommit();

  if (frame_viewer_instrumentation::IsTracingLayerTreeSnapshots()) {
    bool is_new_trace;
    TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
    if (is_new_trace) {
      // We'll be dumping layer trees as part of trace, so make sure
      // PushPropertiesTo() propagates layer debug info to the impl side --
      // otherwise this won't happen for the layers that remain unchanged since
      // tracing started.
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          this, [](Layer* layer) { layer->SetNeedsPushProperties(); });
    }

    for (Layer* layer : LayersThatShouldPushProperties())
      layer->UpdateDebugInfo();
  }
}

void LayerTreeHost::UpdateDeferMainFrameUpdateInternal() {
  proxy_->SetDeferMainFrameUpdate(
      defer_main_frame_update_count_ > 0 ||
      (settings_.enable_surface_synchronization &&
       !local_surface_id_allocation_from_parent_.IsValid()));
}

bool LayerTreeHost::IsUsingLayerLists() const {
  return settings_.use_layer_lists && !force_use_property_tree_builder_;
}

void LayerTreeHost::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHost::SetLayerTreeFrameSink(
    std::unique_ptr<LayerTreeFrameSink> surface) {
  TRACE_EVENT0("cc", "LayerTreeHost::SetLayerTreeFrameSink");
  DCHECK(surface);

  DCHECK(!new_layer_tree_frame_sink_);
  new_layer_tree_frame_sink_ = std::move(surface);
  proxy_->SetLayerTreeFrameSink(new_layer_tree_frame_sink_.get());
}

std::unique_ptr<LayerTreeFrameSink> LayerTreeHost::ReleaseLayerTreeFrameSink() {
  DCHECK(!visible_);

  DidLoseLayerTreeFrameSink();
  proxy_->ReleaseLayerTreeFrameSink();
  return std::move(current_layer_tree_frame_sink_);
}

void LayerTreeHost::RequestNewLayerTreeFrameSink() {
  client_->RequestNewLayerTreeFrameSink();
}

void LayerTreeHost::DidInitializeLayerTreeFrameSink() {
  DCHECK(new_layer_tree_frame_sink_);
  current_layer_tree_frame_sink_ = std::move(new_layer_tree_frame_sink_);
  client_->DidInitializeLayerTreeFrameSink();
}

void LayerTreeHost::DidFailToInitializeLayerTreeFrameSink() {
  DCHECK(new_layer_tree_frame_sink_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new LayerTreeFrameSink after failing to initialize the new one.
  current_layer_tree_frame_sink_ = nullptr;
  new_layer_tree_frame_sink_ = nullptr;
  client_->DidFailToInitializeLayerTreeFrameSink();
}

std::unique_ptr<LayerTreeHostImpl>
LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(task_runner_provider_->IsImplThread());

  const bool supports_impl_scrolling = task_runner_provider_->HasImplThread();
  std::unique_ptr<MutatorHost> mutator_host_impl =
      mutator_host_->CreateImplInstance(supports_impl_scrolling);

  if (!settings_.scroll_animation_duration_for_testing.is_zero()) {
    mutator_host_->SetScrollAnimationDurationForTesting(
        settings_.scroll_animation_duration_for_testing);
  }

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, task_runner_provider_.get(),
      rendering_stats_instrumentation_.get(), task_graph_runner_,
      std::move(mutator_host_impl), id_, std::move(image_worker_task_runner_));
  if (ukm_recorder_factory_) {
    host_impl->InitializeUkm(ukm_recorder_factory_->CreateRecorder());
    ukm_recorder_factory_.reset();
  }

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentHasSlowPaths(content_has_slow_paths_);
  host_impl->SetContentHasNonAAPaint(content_has_non_aa_paint_);
  task_graph_runner_ = nullptr;
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHost::DidLoseLayerTreeFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseLayerTreeFrameSink");
  DCHECK(task_runner_provider_->IsMainThread());
  SetNeedsCommit();
}

ScopedDeferMainFrameUpdate::ScopedDeferMainFrameUpdate(LayerTreeHost* host)
    : host_(host->defer_main_frame_update_weak_ptr_factory_.GetWeakPtr()) {
  host->defer_main_frame_update_count_++;
  host->UpdateDeferMainFrameUpdateInternal();
}

ScopedDeferMainFrameUpdate::~ScopedDeferMainFrameUpdate() {
  LayerTreeHost* host = host_.get();
  if (host) {
    DCHECK_GT(host->defer_main_frame_update_count_, 0u);
    if (--host->defer_main_frame_update_count_ == 0)
      host->UpdateDeferMainFrameUpdateInternal();
  }
}

std::unique_ptr<ScopedDeferMainFrameUpdate>
LayerTreeHost::DeferMainFrameUpdate() {
  return std::make_unique<ScopedDeferMainFrameUpdate>(this);
}

void LayerTreeHost::StartDeferringCommits(base::TimeDelta timeout) {
  proxy_->StartDeferringCommits(timeout);
}

void LayerTreeHost::StopDeferringCommits(PaintHoldingCommitTrigger trigger) {
  proxy_->StopDeferringCommits(trigger);
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

DISABLE_CFI_PERF
void LayerTreeHost::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsCommit() {
  proxy_->SetNeedsCommit();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

bool LayerTreeHost::RequestedMainFramePendingForTesting() {
  return proxy_->RequestedAnimatePending();
}

void LayerTreeHost::SetNeedsRecalculateRasterScales() {
  next_commit_forces_recalculate_raster_scales_ = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

void LayerTreeHost::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHost::SetNeedsCommitWithForcedRedraw() {
  next_commit_forces_redraw_ = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHost::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> events) {
  DCHECK(task_runner_provider_->IsMainThread());
  mutator_host_->SetAnimationEvents(std::move(events));

  // Events are added to a queue to be dispatched but we need a main frame
  // in order to dispatch the events. Also, finished animations require
  // a commit in order to clean up their KeyframeModels but without a main
  // frame we could indefinitely delay cleaning up the animation.
  SetNeedsAnimate();
}

void LayerTreeHost::SetDebugState(
    const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1("cc", "LayerTreeHost::SetHasGpuRasterizationTrigger",
                       TRACE_EVENT_SCOPE_THREAD, "has_trigger",
                       has_gpu_rasterization_trigger_);
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(
    float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  float page_scale = page_scale_factor_ * page_scale_delta;
  SetPageScaleFromImplSide(page_scale);
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

bool LayerTreeHost::IsVisible() const {
  return visible_;
}

void LayerTreeHost::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  RequestMainFrameUpdate();
  UpdateLayers();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time, bool raster) {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());
  proxy->CompositeImmediately(frame_begin_time, raster);
}

bool LayerTreeHost::UpdateLayers() {
  if (!root_layer()) {
    property_trees_.clear();
    return false;
  }

  DCHECK(!root_layer()->parent());
  base::ElapsedTimer timer;

  client_->WillUpdateLayers();
  bool result = DoUpdateLayers();
  client_->DidUpdateLayers();
  micro_benchmark_controller_.DidUpdateLayers();

  if (const char* client_name = GetClientNameForMetrics()) {
    auto elapsed = timer.Elapsed().InMicroseconds();

    std::string histogram_name =
        base::StringPrintf("Compositing.%s.LayersUpdateTime", client_name);
    base::UmaHistogramCounts10M(histogram_name, elapsed);
  }

  return result;
}

void LayerTreeHost::DidPresentCompositorFrame(
    uint32_t frame_token,
    std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
    const gfx::PresentationFeedback& feedback) {
  for (auto& callback : callbacks)
    std::move(callback).Run(feedback);
  client_->DidPresentCompositorFrame(frame_token, feedback);
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

void LayerTreeHost::RecordGpuRasterizationHistogram(
    const LayerTreeHostImpl* host_impl) {
  // Gpu rasterization is only supported for Renderer compositors.
  // Checking for IsSingleThreaded() to exclude Browser compositors.
  if (gpu_rasterization_histogram_recorded_ || IsSingleThreaded())
    return;

  bool gpu_rasterization_enabled = false;
  if (host_impl->layer_tree_frame_sink()) {
    viz::ContextProvider* compositor_context_provider =
        host_impl->layer_tree_frame_sink()->context_provider();
    if (compositor_context_provider) {
      gpu_rasterization_enabled =
          compositor_context_provider->ContextCapabilities().gpu_rasterization;
    }
  }

  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu whitelisting/backlisting into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        gpu_rasterization_enabled);
  if (gpu_rasterization_enabled) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationTriggered",
                          has_gpu_rasterization_trigger_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSuitableContent",
                          !content_has_slow_paths_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSlowPathsWithNonAAPaint",
                          content_has_slow_paths_ && content_has_non_aa_paint_);
  }

  gpu_rasterization_histogram_recorded_ = true;
}

std::string LayerTreeHost::LayersAsString() const {
  std::string layers;
  for (const auto* layer : *this)
    layers += layer->ToString() + "\n";
  return layers;
}

bool LayerTreeHost::CaptureContent(std::vector<NodeId>* content) {
  if (viewport_visible_rect_.IsEmpty())
    return false;

  gfx::Rect rect = gfx::Rect(viewport_visible_rect_.width(),
                             viewport_visible_rect_.height());
  for (auto* layer : *this)
    layer->CaptureContent(rect, content);

  return true;
}

bool LayerTreeHost::DoUpdateLayers() {
  TRACE_EVENT1("cc,benchmark", "LayerTreeHost::DoUpdateLayers",
               "source_frame_number", SourceFrameNumber());

  UpdateHudLayer(debug_state_.ShowHudInfo());

  // The non-layer-list mode is used when blink provides cc with a layer tree
  // and cc needs to compute property trees from that.
  // In layer lists mode, blink sends cc property trees directly so they do not
  // need to be built here. Layer lists mode is used by BlinkGenPropertyTrees
  // and CompositeAfterPaint.
  if (!IsUsingLayerLists()) {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::BuildPropertyTrees");
    Layer* root_scroll =
        PropertyTreeBuilder::FindFirstScrollableLayer(root_layer_.get());
    Layer* page_scale_layer = viewport_layers_.page_scale.get();
    if (!page_scale_layer && root_scroll)
      page_scale_layer = root_scroll->parent();
    gfx::Transform identity_transform;
    PropertyTreeBuilder::BuildPropertyTrees(
        root_layer_.get(), page_scale_layer, inner_viewport_scroll_layer(),
        outer_viewport_scroll_layer(), overscroll_elasticity_element_id(),
        elastic_overscroll_, page_scale_factor_, device_scale_factor_,
        gfx::Rect(device_viewport_size_), identity_transform, &property_trees_);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "LayerTreeHost::UpdateLayers_BuiltPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees_.AsTracedValue());
  } else {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                         "LayerTreeHost::UpdateLayers_ReceivedPropertyTrees",
                         TRACE_EVENT_SCOPE_THREAD, "property_trees",
                         property_trees_.AsTracedValue());
    // The HUD layer is managed outside the layer list sent to LayerTreeHost
    // and needs to have its property tree state set.
    if (hud_layer_ && root_layer_.get()) {
      hud_layer_->SetTransformTreeIndex(root_layer_->transform_tree_index());
      hud_layer_->SetEffectTreeIndex(root_layer_->effect_tree_index());
      hud_layer_->SetClipTreeIndex(root_layer_->clip_tree_index());
      hud_layer_->SetScrollTreeIndex(root_layer_->scroll_tree_index());
      hud_layer_->set_property_tree_sequence_number(
          root_layer_->property_tree_sequence_number());
    }
  }

#if DCHECK_IS_ON()
  // Ensure property tree nodes were created for all layers. When using layer
  // lists, this can fail if blink doesn't setup layers or nodes correctly in
  // |PaintArtifactCompositor|. When not using layer lists, this can fail if
  // |PropertyTreeBuilder::BuildPropertyTrees| fails to create property tree
  // nodes.
  for (auto* layer : *this) {
    DCHECK(property_trees_.effect_tree.Node(layer->effect_tree_index()));
    DCHECK(property_trees_.transform_tree.Node(layer->transform_tree_index()));
    DCHECK(property_trees_.clip_tree.Node(layer->clip_tree_index()));
    DCHECK(property_trees_.scroll_tree.Node(layer->scroll_tree_index()));
  }
#else
  // This is a quick sanity check for readiness of paint properties.
  // TODO(crbug.com/913464): This is to help analysis of crashes of the bug.
  // Remove this CHECK when we close the bug.
  CHECK(property_trees_.effect_tree.Node(root_layer_->effect_tree_index()));
#endif

  draw_property_utils::UpdatePropertyTrees(this, &property_trees_);

  LayerList update_layer_list;
  draw_property_utils::FindLayersThatNeedUpdates(this, &property_trees_,
                                                 &update_layer_list);

  bool painted_content_has_slow_paths = false;
  bool painted_content_has_non_aa_paint = false;
  bool did_paint_content =
      PaintContent(update_layer_list, &painted_content_has_slow_paths,
                   &painted_content_has_non_aa_paint);

  // |painted_content_has_non_aa_paint| is a correctness (not performance)
  // modifier, if it changes we immediately update. To prevent churn, this flag
  // is sticky.
  content_has_non_aa_paint_ |= painted_content_has_non_aa_paint;

  // If no slow-path content has appeared for a required number of frames,
  // update the flag.
  if (!painted_content_has_slow_paths) {
    ++num_consecutive_frames_without_slow_paths_;
    if (num_consecutive_frames_without_slow_paths_ >=
        kNumFramesToConsiderBeforeRemovingSlowPathFlag) {
      content_has_slow_paths_ = false;
    }
  } else {
    num_consecutive_frames_without_slow_paths_ = 0;
    content_has_slow_paths_ = true;
  }

  return did_paint_content;
}

void LayerTreeHost::ApplyViewportChanges(const ScrollAndScaleSet& info) {
  gfx::ScrollOffset inner_viewport_scroll_delta;
  if (info.inner_viewport_scroll.element_id)
    inner_viewport_scroll_delta = info.inner_viewport_scroll.scroll_delta;

  if (inner_viewport_scroll_delta.IsZero() && info.page_scale_delta == 1.f &&
      info.elastic_overscroll_delta.IsZero() && !info.top_controls_delta &&
      !info.browser_controls_constraint_changed &&
      !info.scroll_gesture_did_end &&
      info.is_pinch_gesture_active == is_pinch_gesture_active_from_impl_) {
    return;
  }
  is_pinch_gesture_active_from_impl_ = info.is_pinch_gesture_active;

  // Preemptively apply the scroll offset and scale delta here before sending
  // it to the client.  If the client comes back and sets it to the same
  // value, then the layer can early out without needing a full commit.
  if (viewport_layers_.inner_viewport_scroll) {
    viewport_layers_.inner_viewport_scroll->SetScrollOffsetFromImplSide(
        viewport_layers_.inner_viewport_scroll->CurrentScrollOffset() +
        inner_viewport_scroll_delta);
  }

  ApplyPageScaleDeltaFromImplSide(info.page_scale_delta);
  SetElasticOverscrollFromImplSide(elastic_overscroll_ +
                                   info.elastic_overscroll_delta);
  // TODO(ccameron): pass the elastic overscroll here so that input events
  // may be translated appropriately.
  client_->ApplyViewportChanges(
      {inner_viewport_scroll_delta, info.elastic_overscroll_delta,
       info.page_scale_delta, info.is_pinch_gesture_active,
       info.top_controls_delta, info.browser_controls_constraint,
       info.scroll_gesture_did_end});
  SetNeedsUpdateLayers();
}

void LayerTreeHost::RecordManipulationTypeCounts(
    const ScrollAndScaleSet& scroll_info) {
  client_->RecordManipulationTypeCounts(scroll_info.manipulation_info);
}

void LayerTreeHost::SendOverscrollAndScrollEndEventsFromImplSide(
    const ScrollAndScaleSet& info) {
  if (info.scroll_latched_element_id == ElementId())
    return;

  if (!info.overscroll_delta.IsZero()) {
    client_->SendOverscrollEventFromImplSide(info.overscroll_delta,
                                             info.scroll_latched_element_id);
  }
  if (info.scroll_gesture_did_end)
    client_->SendScrollEndEventFromImplSide(info.scroll_latched_element_id);
}

void LayerTreeHost::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  DCHECK(info);
  for (auto& swap_promise : info->swap_promises) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Main thread scroll update");
    swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
  }

  if (root_layer_) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = LayerByElementId(info->scrolls[i].element_id);
      if (!layer)
        continue;
      layer->SetScrollOffsetFromImplSide(layer->CurrentScrollOffset() +
                                         info->scrolls[i].scroll_delta);
      SetNeedsUpdateLayers();
    }
    for (size_t i = 0; i < info->scrollbars.size(); ++i) {
      Layer* layer = LayerByElementId(info->scrollbars[i].element_id);
      if (!layer)
        continue;
      layer->SetScrollbarsHiddenFromImplSide(info->scrollbars[i].hidden);
    }
  }

  SendOverscrollAndScrollEndEventsFromImplSide(*info);

  // This needs to happen after scroll deltas have been sent to prevent top
  // controls from clamping the layout viewport both on the compositor and
  // on the main thread.
  ApplyViewportChanges(*info);

  RecordManipulationTypeCounts(*info);
}

void LayerTreeHost::RecordStartOfFrameMetrics() {
  client_->RecordStartOfFrameMetrics();
}

void LayerTreeHost::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  client_->RecordEndOfFrameMetrics(frame_begin_time);
}

const base::WeakPtr<InputHandler>& LayerTreeHost::GetInputHandler()
    const {
  return input_handler_weak_ptr_;
}

void LayerTreeHost::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  // Browser controls are only used in threaded mode but Blink layout tests may
  // call into this. The single threaded version is a no-op.
  proxy_->UpdateBrowserControlsState(constraints, current, animate);
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  std::unique_ptr<MutatorEvents> events = mutator_host_->CreateEvents();

  if (mutator_host_->TickAnimations(monotonic_time,
                                    property_trees()->scroll_tree, true))
    mutator_host_->UpdateAnimationState(true, events.get());

  if (!events->IsEmpty()) {
    // If not using layer lists, animation state changes will require
    // rebuilding property trees to track them.
    if (!IsUsingLayerLists())
      property_trees_.needs_rebuild = true;

    // A commit is required to push animation changes to the compositor.
    SetNeedsCommit();
  }
}

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    MicroBenchmark::DoneCallback callback) {
  return micro_benchmark_controller_.ScheduleRun(
      benchmark_name, std::move(value), std::move(callback));
}

bool LayerTreeHost::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, std::move(value));
}

void LayerTreeHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  // The animation worklet system assumes that the mutator will never be called
  // from the main thread, which will not be the case if we're running in
  // single-threaded mode.
  if (!task_runner_provider_->HasImplThread()) {
    DLOG(ERROR) << "LayerTreeMutator not supported in single-thread mode";
    return;
  }
  proxy_->SetMutator(std::move(mutator));
}

void LayerTreeHost::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  // The paint worklet system assumes that the painter will never be called from
  // the main thread, which will not be the case if we're running in
  // single-threaded mode.
  DCHECK(task_runner_provider_->HasImplThread())
      << "PaintWorkletLayerPainter not supported in single-thread mode";
  proxy_->SetPaintWorkletLayerPainter(std::move(painter));
}

bool LayerTreeHost::IsSingleThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::SINGLE_THREADED ||
         !task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::SINGLE_THREADED;
}

bool LayerTreeHost::IsThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::THREADED ||
         task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::THREADED;
}

void LayerTreeHost::RequestPresentationTimeForNextFrame(
    PresentationTimeCallback callback) {
  pending_presentation_time_callbacks_.push_back(std::move(callback));
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(nullptr);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(this);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  content_has_slow_paths_ = false;
  content_has_non_aa_paint_ = false;
  gpu_rasterization_histogram_recorded_ = false;

  force_use_property_tree_builder_ = false;

  SetNeedsFullTreeSync();
}

void LayerTreeHost::SetNonBlinkManagedRootLayer(
    scoped_refptr<Layer> root_layer) {
  SetRootLayer(std::move(root_layer));

  DCHECK(root_layer_->children().empty());
  if (IsUsingLayerLists() && root_layer_)
    force_use_property_tree_builder_ = true;
}

void LayerTreeHost::RegisterViewportLayers(const ViewportLayers& layers) {
  DCHECK(!layers.inner_viewport_scroll ||
         layers.inner_viewport_scroll != layers.outer_viewport_scroll);
  // The page scale layer only affects the innter viewport scroll layer. The
  // page scale layer should be sandwiched between the inner viewport scroll and
  // inner viewport container layers:
  //   inner viewport container
  //     overscroll elasticity (optional)
  //       page scale
  //         inner viewport scroll
  DCHECK(IsUsingLayerLists() || !layers.page_scale ||
         layers.inner_viewport_scroll->parent() == layers.page_scale);
  DCHECK(IsUsingLayerLists() || !layers.page_scale ||
         layers.page_scale->parent()->element_id() ==
             layers.overscroll_elasticity_element_id ||
         layers.page_scale->parent() == layers.inner_viewport_container);
  viewport_layers_.overscroll_elasticity_element_id =
      layers.overscroll_elasticity_element_id;
  viewport_layers_.page_scale = layers.page_scale;
  viewport_layers_.inner_viewport_container = layers.inner_viewport_container;
  viewport_layers_.outer_viewport_container = layers.outer_viewport_container;
  viewport_layers_.inner_viewport_scroll = layers.inner_viewport_scroll;
  viewport_layers_.outer_viewport_scroll = layers.outer_viewport_scroll;
}

void LayerTreeHost::RegisterViewportPropertyIds(
    const ViewportPropertyIds& ids) {
  DCHECK(!viewport_layers_.inner_viewport_scroll);
  DCHECK(IsUsingLayerLists());
  viewport_property_ids_ = ids;
}

void LayerTreeHost::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  selection_ = selection;
  SetNeedsCommit();
}

void LayerTreeHost::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (have_scroll_event_handlers_ == have_event_handlers)
    return;

  have_scroll_event_handlers_ = have_event_handlers;
  SetNeedsCommit();
}

void LayerTreeHost::SetEventListenerProperties(
    EventListenerClass event_class,
    EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (event_listener_properties_[index] == properties)
    return;

  // If the mouse wheel event listener is blocking, then every layer in the
  // layer tree sets a wheel event handler region to be its entire bounds,
  // otherwise it sets it to empty.
  //
  // Thus when it changes, we want to request every layer to push properties
  // and recompute its wheel event handler region, since the computation is
  // done in PushPropertiesTo.
  if (event_class == EventListenerClass::kMouseWheel) {
    bool new_property_is_blocking =
        properties == EventListenerProperties::kBlocking ||
        properties == EventListenerProperties::kBlockingAndPassive;
    EventListenerProperties old_properties = event_listener_properties_[index];
    bool old_property_is_blocking =
        old_properties == EventListenerProperties::kBlocking ||
        old_properties == EventListenerProperties::kBlockingAndPassive;

    if (old_property_is_blocking != new_property_is_blocking) {
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          this, [](Layer* layer) { layer->SetNeedsPushProperties(); });
    }
  }

  event_listener_properties_[index] = properties;
  SetNeedsCommit();
}

void LayerTreeHost::SetViewportSizeAndScale(
    const gfx::Size& device_viewport_size,
    float device_scale_factor,
    const viz::LocalSurfaceIdAllocation&
        local_surface_id_allocation_from_parent) {
  const viz::LocalSurfaceId previous_local_surface_id =
      local_surface_id_allocation_from_parent_.local_surface_id();
  SetLocalSurfaceIdAllocationFromParent(
      local_surface_id_allocation_from_parent);

  bool device_viewport_size_changed = false;
  if (device_viewport_size_ != device_viewport_size) {
    device_viewport_size_ = device_viewport_size;
    device_viewport_size_changed = true;
  }
  bool painted_device_scale_factor_changed = false;
  bool device_scale_factor_changed = false;
  if (settings_.use_painted_device_scale_factor) {
    DCHECK_EQ(device_scale_factor_, 1.f);
    if (painted_device_scale_factor_ != device_scale_factor) {
      painted_device_scale_factor_ = device_scale_factor;
      painted_device_scale_factor_changed = true;
    }
  } else {
    DCHECK_EQ(painted_device_scale_factor_, 1.f);
    if (device_scale_factor_ != device_scale_factor) {
      device_scale_factor_ = device_scale_factor;
      device_scale_factor_changed = true;
    }
  }

  if (device_viewport_size_changed || painted_device_scale_factor_changed ||
      device_scale_factor_changed) {
    SetPropertyTreesNeedRebuild();
    SetNeedsCommit();
#if defined(OS_MACOSX)
    // TODO(jonross): This check is not valid on Aura or Mus yet, but should be.
    CHECK(!has_pushed_local_surface_id_from_parent_ ||
          new_local_surface_id_request_ ||
          !local_surface_id_allocation_from_parent_.IsValid())
        << "Invalid Surface Id State: !has_pushed "
        << !has_pushed_local_surface_id_from_parent_ << " new_id_request "
        << new_local_surface_id_request_ << " !valid_parent_id "
        << !local_surface_id_allocation_from_parent_.IsValid()
        << ". Changed state: device_viewport_size "
        << device_viewport_size_changed << " painted_device_scale_factor "
        << painted_device_scale_factor_changed << " device_scale_factor "
        << device_scale_factor_changed << " cached LSId "
        << previous_local_surface_id.ToString() << " new LSId "
        << local_surface_id_allocation_from_parent.ToString();
#endif
  }
}

void LayerTreeHost::SetViewportVisibleRect(const gfx::Rect& visible_rect) {
  if (visible_rect == viewport_visible_rect_)
    return;

  viewport_visible_rect_ = visible_rect;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsHeight(float top_height,
                                             float bottom_height,
                                             bool shrink) {
  if (top_controls_height_ == top_height &&
      bottom_controls_height_ == bottom_height &&
      browser_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_height_ = top_height;
  bottom_controls_height_ = bottom_height;
  browser_controls_shrink_blink_size_ = shrink;
  SetNeedsCommit();
}

void LayerTreeHost::SetBrowserControlsShownRatio(float ratio) {
  if (top_controls_shown_ratio_ == ratio)
    return;

  top_controls_shown_ratio_ = ratio;
  SetNeedsCommit();
}

void LayerTreeHost::SetOverscrollBehavior(const OverscrollBehavior& behavior) {
  if (overscroll_behavior_ == behavior)
    return;
  overscroll_behavior_ = behavior;
  SetNeedsCommit();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor_ == page_scale_factor &&
      min_page_scale_factor_ == min_page_scale_factor &&
      max_page_scale_factor_ == max_page_scale_factor)
    return;
  DCHECK_GE(page_scale_factor, min_page_scale_factor);
  DCHECK_LE(page_scale_factor, max_page_scale_factor);
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this dcheck as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(!settings_.is_layer_tree_for_subframe ||
         page_scale_factor == page_scale_factor_)
      << "Setting PSF in oopif subframe: old psf = " << page_scale_factor_
      << ", new psf = " << page_scale_factor;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(new PendingPageScaleAnimation(
      target_offset, use_anchor, scale, duration));

  SetNeedsCommit();
}

bool LayerTreeHost::HasPendingPageScaleAnimation() const {
  return !!pending_page_scale_animation_.get();
}

void LayerTreeHost::SetRecordingScaleFactor(float recording_scale_factor) {
  if (recording_scale_factor_ == recording_scale_factor)
    return;
  recording_scale_factor_ = recording_scale_factor;
}

void LayerTreeHost::SetRasterColorSpace(
    const gfx::ColorSpace& raster_color_space) {
  if (raster_color_space_ == raster_color_space)
    return;
  raster_color_space_id_ = gfx::ColorSpace::GetNextId();
  raster_color_space_ = raster_color_space;
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      this, [](Layer* layer) { layer->SetNeedsDisplay(); });
}

void LayerTreeHost::SetExternalPageScaleFactor(
    float page_scale_factor,
    bool is_external_pinch_gesture_active) {
  if (external_page_scale_factor_ == page_scale_factor &&
      is_external_pinch_gesture_active_ == is_external_pinch_gesture_active) {
    return;
  }

  external_page_scale_factor_ = page_scale_factor;
  is_external_pinch_gesture_active_ = is_external_pinch_gesture_active;
  SetNeedsCommit();
}

void LayerTreeHost::ClearCachesOnNextCommit() {
  clear_caches_on_next_commit_ = true;
}

void LayerTreeHost::SetLocalSurfaceIdAllocationFromParent(
    const viz::LocalSurfaceIdAllocation&
        local_surface_id_allocation_from_parent) {
  const viz::LocalSurfaceId& local_surface_id_from_parent =
      local_surface_id_allocation_from_parent.local_surface_id();
  const viz::LocalSurfaceId current_local_surface_id_from_parent =
      local_surface_id_allocation_from_parent_.local_surface_id();

  // If the viz::LocalSurfaceId is valid but the allocation time is invalid then
  // this API is not being used correctly.
  DCHECK_EQ(local_surface_id_from_parent.is_valid(),
            local_surface_id_allocation_from_parent.IsValid());

  // These traces are split into two due to the usage of TRACE_ID_GLOBAL for the
  // incoming flow (it comes from a different process), and TRACE_ID_LOCAL for
  // the outgoing flow. The outgoing flow uses local to ensure that it doesn't
  // flow into the wrong trace in different process.
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(local_surface_id_from_parent.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN, "step", "SetLocalSurfaceAllocationIdFromParent",
      "local_surface_id_allocation",
      local_surface_id_allocation_from_parent.ToString());
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_LOCAL(local_surface_id_from_parent.submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SetLocalSurfaceAllocationIdFromParent", "local_surface_id_allocation",
      local_surface_id_allocation_from_parent.ToString());
  // Always update the cached state of the viz::LocalSurfaceId to reflect the
  // latest value received from our parent.
  local_surface_id_allocation_from_parent_ =
      local_surface_id_allocation_from_parent;
  has_pushed_local_surface_id_from_parent_ = false;

  // If the parent sequence number has not advanced, then there is no need to
  // commit anything. This can occur when the child sequence number has
  // advanced. Which means that child has changed visual properites, and the
  // parent agreed upon these without needing to further advance its sequence
  // number. When this occurs the child is already up-to-date and a commit here
  // is simply redundant.
  if (AreEmbedTokensEqual(current_local_surface_id_from_parent,
                          local_surface_id_from_parent) &&
      AreParentSequencesEqual(current_local_surface_id_from_parent,
                              local_surface_id_from_parent)) {
    return;
  }
  UpdateDeferMainFrameUpdateInternal();
  SetNeedsCommit();
}

void LayerTreeHost::RequestNewLocalSurfaceId() {
  // If surface synchronization is enabled, then we can still request a new
  // viz::LocalSurfaceId but that request will be deferred until we have a valid
  // viz::LocalSurfaceId from the parent.
  DCHECK(settings_.enable_surface_synchronization ||
         local_surface_id_allocation_from_parent_.IsValid());
  if (new_local_surface_id_request_)
    return;
  new_local_surface_id_request_ = true;
  SetNeedsCommit();
}

bool LayerTreeHost::TakeNewLocalSurfaceIdRequest() {
  bool new_local_surface_id_request = new_local_surface_id_request_;
  new_local_surface_id_request_ = false;
  return new_local_surface_id_request;
}

void LayerTreeHost::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (!IsUsingLayerLists() && layer->element_id()) {
    mutator_host_->RegisterElementId(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
}

void LayerTreeHost::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (!IsUsingLayerLists() && layer->element_id()) {
    mutator_host_->UnregisterElementId(layer->element_id(),
                                       ElementListType::ACTIVE);
  }
  layers_that_should_push_properties_.erase(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTreeHost::LayerById(int id) const {
  auto iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

bool LayerTreeHost::PaintContent(const LayerList& update_layer_list,
                                 bool* content_has_slow_paths,
                                 bool* content_has_non_aa_paint) {
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    *content_has_slow_paths |= layer->HasSlowPaths();
    *content_has_non_aa_paint |= layer->HasNonAAPaint();
  }
  return did_paint_content;
}

void LayerTreeHost::AddSurfaceRange(const viz::SurfaceRange& surface_range) {
  if (++surface_ranges_[surface_range] == 1)
    needs_surface_ranges_sync_ = true;
}

void LayerTreeHost::RemoveSurfaceRange(const viz::SurfaceRange& surface_range) {
  auto iter = surface_ranges_.find(surface_range);
  if (iter == surface_ranges_.end())
    return;

  if (--iter->second <= 0) {
    surface_ranges_.erase(iter);
    needs_surface_ranges_sync_ = true;
  }
}

base::flat_set<viz::SurfaceRange> LayerTreeHost::SurfaceRanges() const {
  base::flat_set<viz::SurfaceRange> ranges;
  for (auto& map_entry : surface_ranges_)
    ranges.insert(map_entry.first);
  return ranges;
}

void LayerTreeHost::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeHost::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

void LayerTreeHost::ClearLayersThatShouldPushProperties() {
  layers_that_should_push_properties_.clear();
}

void LayerTreeHost::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(CommitRequested());
  // We should never process non-unit page_scale_delta for an OOPIF subframe.
  // TODO(wjmaclean): Remove this check as a pre-condition to closing the bug.
  // https://crbug.com/845097
  DCHECK(!settings_.is_layer_tree_for_subframe ||
         page_scale == page_scale_factor_)
      << "Setting PSF in oopif subframe: old psf = " << page_scale_factor_
      << ", new psf = " << page_scale;
  page_scale_factor_ = page_scale;
  SetPropertyTreesNeedRebuild();
}

void LayerTreeHost::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(CommitRequested());
  elastic_overscroll_ = elastic_overscroll;
}

void LayerTreeHost::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer_.get())
      hud_layer_ = HeadsUpDisplayLayer::Create();
    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
    hud_layer_->UpdateLocationAndSize(device_viewport_size_,
                                      device_scale_factor_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = nullptr;
  }
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  SetNeedsUpdateLayers();
}

void LayerTreeHost::PushLayerTreePropertiesTo(LayerTreeImpl* tree_impl) {
  tree_impl->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = tree_impl->LayerById(hud_layer_->id());
    tree_impl->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    tree_impl->set_hud_layer(nullptr);
  }

  tree_impl->set_background_color(background_color_);
  tree_impl->set_have_scroll_event_handlers(have_scroll_event_handlers_);
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (viewport_layers_.inner_viewport_scroll) {
    LayerTreeImpl::ViewportLayerIds ids;
    if (viewport_layers_.overscroll_elasticity_element_id) {
      ids.overscroll_elasticity_element_id =
          viewport_layers_.overscroll_elasticity_element_id;
    }
    ids.page_scale = viewport_layers_.page_scale->id();
    if (viewport_layers_.inner_viewport_container)
      ids.inner_viewport_container =
          viewport_layers_.inner_viewport_container->id();
    if (viewport_layers_.outer_viewport_container)
      ids.outer_viewport_container =
          viewport_layers_.outer_viewport_container->id();
    ids.inner_viewport_scroll = viewport_layers_.inner_viewport_scroll->id();
    if (viewport_layers_.outer_viewport_scroll)
      ids.outer_viewport_scroll = viewport_layers_.outer_viewport_scroll->id();
    tree_impl->SetViewportLayersFromIds(ids);
    DCHECK(IsUsingLayerLists() || viewport_layers_.inner_viewport_scroll
                                      ->IsContainerForFixedPositionLayers());
  } else {
    tree_impl->ClearViewportLayers();
  }

  tree_impl->set_viewport_property_ids(viewport_property_ids_);

  tree_impl->RegisterSelection(selection_);

  tree_impl->PushPageScaleFromMainThread(
      page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);

  tree_impl->set_browser_controls_shrink_blink_size(
      browser_controls_shrink_blink_size_);
  tree_impl->SetTopControlsHeight(top_controls_height_);
  tree_impl->SetBottomControlsHeight(bottom_controls_height_);
  tree_impl->set_overscroll_behavior(overscroll_behavior_);
  tree_impl->PushBrowserControlsFromMainThread(top_controls_shown_ratio_);
  tree_impl->elastic_overscroll()->PushMainToPending(elastic_overscroll_);
  if (tree_impl->IsActiveTree())
    tree_impl->elastic_overscroll()->PushPendingToActive();

  tree_impl->SetRasterColorSpace(raster_color_space_id_, raster_color_space_);
  tree_impl->SetExternalPageScaleFactor(external_page_scale_factor_);

  tree_impl->set_painted_device_scale_factor(painted_device_scale_factor_);
  tree_impl->SetDeviceScaleFactor(device_scale_factor_);
  tree_impl->SetDeviceViewportSize(device_viewport_size_);
  tree_impl->SetViewportVisibleRect(viewport_visible_rect_);

  if (TakeNewLocalSurfaceIdRequest())
    tree_impl->RequestNewLocalSurfaceId();

  tree_impl->SetLocalSurfaceIdAllocationFromParent(
      local_surface_id_allocation_from_parent_);
  has_pushed_local_surface_id_from_parent_ = true;

  if (pending_page_scale_animation_) {
    tree_impl->SetPendingPageScaleAnimation(
        std::move(pending_page_scale_animation_));
  }

  if (TakeForceSendMetadataRequest())
    tree_impl->RequestForceSendMetadata();

  tree_impl->set_has_ever_been_drawn(false);
}

void LayerTreeHost::PushSurfaceRangesTo(LayerTreeImpl* tree_impl) {
  if (needs_surface_ranges_sync()) {
    tree_impl->ClearSurfaceRanges();
    tree_impl->SetSurfaceRanges(SurfaceRanges());
    // Reset for next update
    set_needs_surface_ranges_sync(false);
  }
}

void LayerTreeHost::PushLayerTreeHostPropertiesTo(
    LayerTreeHostImpl* host_impl) {
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentHasSlowPaths(content_has_slow_paths_);
  host_impl->SetContentHasNonAAPaint(content_has_non_aa_paint_);
  host_impl->set_external_pinch_gesture_active(
      is_external_pinch_gesture_active_);
  RecordGpuRasterizationHistogram(host_impl);

  host_impl->SetDebugState(debug_state_);
}

Layer* LayerTreeHost::LayerByElementId(ElementId element_id) const {
  auto iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTreeHost::RegisterElement(ElementId element_id,
                                    ElementListType list_type,
                                    Layer* layer) {
  // When using layer lists only scrollable layers should be registered.
  DCHECK(!IsUsingLayerLists() || layer->inputs_.scrollable);
  element_layers_map_[element_id] = layer;

  // Animation ElementIds are unregistered by |SetActiveRegisteredElementIds|
  // when using layer lists.
  if (!IsUsingLayerLists())
    mutator_host_->RegisterElementId(element_id, list_type);
}

void LayerTreeHost::UnregisterElement(ElementId element_id,
                                      ElementListType list_type) {
  // Animation ElementIds are unregistered by |SetActiveRegisteredElementIds|
  // when using layer lists.
  if (!IsUsingLayerLists())
    mutator_host_->UnregisterElementId(element_id, list_type);

  element_layers_map_.erase(element_id);
}

void LayerTreeHost::UpdateActiveElements() {
  DCHECK(IsUsingLayerLists());
  mutator_host_->UpdateRegisteredElementIds(ElementListType::ACTIVE);
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeHost::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

void LayerTreeHost::BuildPropertyTreesForTesting() {
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer(), page_scale_layer(), inner_viewport_scroll_layer(),
      outer_viewport_scroll_layer(), overscroll_elasticity_element_id(),
      elastic_overscroll(), page_scale_factor(), device_scale_factor(),
      gfx::Rect(device_viewport_size()), identity_transform, property_trees());
}

bool LayerTreeHost::IsElementInPropertyTrees(ElementId element_id,
                                             ElementListType list_type) const {
  if (IsUsingLayerLists()) {
    return list_type == ElementListType::ACTIVE &&
           property_trees()->HasElement(element_id);
  }
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTreeHost::SetMutatorsNeedCommit() {
  SetNeedsCommit();
}

void LayerTreeHost::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTreeHost::SetElementFilterMutated(ElementId element_id,
                                            ElementListType list_type,
                                            const FilterOperations& filters) {
  if (IsUsingLayerLists()) {
    // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
    // tree nodes and can set the filter directly on the effect node.
    property_trees_.effect_tree.OnFilterAnimated(element_id, filters);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTreeHost::SetElementBackdropFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& backdrop_filters) {
  if (IsUsingLayerLists()) {
    // In BlinkGenPropertyTrees/CompositeAfterPaint we always have property
    // tree nodes and can set the backdrop_filter directly on the effect node.
    property_trees_.effect_tree.OnBackdropFilterAnimated(element_id,
                                                         backdrop_filters);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnBackdropFilterAnimated(backdrop_filters);
}

void LayerTreeHost::SetElementOpacityMutated(ElementId element_id,
                                             ElementListType list_type,
                                             float opacity) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);

  if (IsUsingLayerLists()) {
    property_trees_.effect_tree.OnOpacityAnimated(element_id, opacity);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);

  if (EffectNode* node =
          property_trees_.effect_tree.Node(layer->effect_tree_index())) {
    DCHECK_EQ(layer->effect_tree_index(), node->id);
    if (node->opacity == opacity)
      return;

    node->opacity = opacity;
    property_trees_.effect_tree.set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (IsUsingLayerLists()) {
    property_trees_.transform_tree.OnTransformAnimated(element_id, transform);
    return;
  }

  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);

  if (layer->has_transform_node()) {
    TransformNode* node =
        property_trees_.transform_tree.Node(layer->transform_tree_index());
    if (node->local == transform)
      return;

    node->local = transform;
    node->needs_local_transform_update = true;
    node->has_potential_animation = true;
    property_trees_.transform_tree.set_needs_update(true);
  }

  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  // Do nothing. Scroll deltas will be sent from the compositor thread back
  // to the main thread in the same manner as during non-animated
  // compositor-driven scrolling.
}

void LayerTreeHost::ElementIsAnimatingChanged(
    const PropertyToElementIdMap& element_id_map,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state) {
  DCHECK_EQ(ElementListType::ACTIVE, list_type);
  property_trees()->ElementIsAnimatingChanged(element_id_map, mask, state,
                                              true);
}

void LayerTreeHost::AnimationScalesChanged(ElementId element_id,
                                           ElementListType list_type,
                                           float maximum_scale,
                                           float starting_scale) {
  DCHECK_EQ(ElementListType::ACTIVE, list_type);
  property_trees()->AnimationScalesChanged(element_id, maximum_scale,
                                           starting_scale);
}

gfx::ScrollOffset LayerTreeHost::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  return property_trees()->scroll_tree.current_scroll_offset(element_id);
}

void LayerTreeHost::QueueImageDecode(const PaintImage& image,
                                     base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("cc", "LayerTreeHost::QueueImageDecode");
  queued_image_decodes_.emplace_back(image, std::move(callback));
  SetNeedsCommit();
}

LayerListIterator<Layer> LayerTreeHost::begin() const {
  return LayerListIterator<Layer>(root_layer_.get());
}

LayerListIterator<Layer> LayerTreeHost::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTreeHost::rbegin() {
  return LayerListReverseIterator<Layer>(root_layer_.get());
}

LayerListReverseIterator<Layer> LayerTreeHost::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTreeHost::SetPropertyTreesForTesting(
    const PropertyTrees* property_trees) {
  property_trees_ = *property_trees;
}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

void LayerTreeHost::SetHasCopyRequest(bool has_copy_request) {
  has_copy_request_ = has_copy_request;
}

void LayerTreeHost::RequestBeginMainFrameNotExpected(bool new_state) {
  proxy_->RequestBeginMainFrameNotExpected(new_state);
}

void LayerTreeHost::SetSourceURL(ukm::SourceId source_id, const GURL& url) {
  proxy_->SetSourceURL(source_id, url);
}

void LayerTreeHost::SetRenderFrameObserver(
    std::unique_ptr<RenderFrameMetadataObserver> observer) {
  proxy_->SetRenderFrameObserver(std::move(observer));
}

bool LayerTreeHost::TakeForceSendMetadataRequest() {
  bool force_send_metadata_request = force_send_metadata_request_;
  force_send_metadata_request_ = false;
  return force_send_metadata_request;
}

}  // namespace cc
