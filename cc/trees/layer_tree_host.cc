// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/base/math_util.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/blimp/picture_data.h"
#include "cc/blimp/picture_data_conversions.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/layer_proto_converter.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree.pb.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/remote_channel_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace {
static base::StaticAtomicSequenceNumber s_layer_tree_host_sequence_number;
}

namespace cc {
namespace {

std::unique_ptr<base::trace_event::TracedValue>
ComputeLayerTreeHostProtoSizeSplitAsValue(proto::LayerTreeHost* proto) {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  base::CheckedNumeric<int> base_layer_properties_size = 0;
  base::CheckedNumeric<int> picture_layer_properties_size = 0;
  base::CheckedNumeric<int> display_item_list_size = 0;
  base::CheckedNumeric<int> drawing_display_items_size = 0;

  const proto::LayerUpdate& layer_update_proto = proto->layer_updates();
  for (int i = 0; i < layer_update_proto.layers_size(); ++i) {
    const proto::LayerProperties layer_properties_proto =
        layer_update_proto.layers(i);
    base_layer_properties_size += layer_properties_proto.base().ByteSize();

    if (layer_properties_proto.has_picture()) {
      const proto::PictureLayerProperties& picture_proto =
          layer_properties_proto.picture();
      picture_layer_properties_size += picture_proto.ByteSize();

      const proto::DisplayItemList& display_list_proto =
          picture_proto.display_list();
      display_item_list_size += display_list_proto.ByteSize();

      for (int j = 0; j < display_list_proto.items_size(); ++j) {
        const proto::DisplayItem& display_item = display_list_proto.items(j);
        if (display_item.type() == proto::DisplayItem::Type_Drawing)
          drawing_display_items_size += display_item.ByteSize();
      }
    }
  }

  value->SetInteger("TotalLayerTreeHostProtoSize", proto->ByteSize());
  value->SetInteger("LayerTreeHierarchySize",
                    proto->layer_tree().root_layer().ByteSize());
  value->SetInteger("LayerUpdatesSize", proto->layer_updates().ByteSize());
  value->SetInteger("PropertyTreesSize",
                    proto->layer_tree().property_trees().ByteSize());

  // LayerUpdate size breakdown.
  value->SetInteger("TotalBasePropertiesSize",
                    base_layer_properties_size.ValueOrDefault(-1));
  value->SetInteger("PictureLayerPropertiesSize",
                    picture_layer_properties_size.ValueOrDefault(-1));
  value->SetInteger("DisplayItemListSize",
                    display_item_list_size.ValueOrDefault(-1));
  value->SetInteger("DrawingDisplayItemsSize",
                    drawing_display_items_size.ValueOrDefault(-1));
  return value;
}

}  // namespace

LayerTreeHost::InitParams::InitParams() {
}

LayerTreeHost::InitParams::~InitParams() {
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(impl_task_runner.get());
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::THREADED));
  layer_tree_host->InitializeThreaded(
      params->main_task_runner, impl_task_runner,
      std::move(params->external_begin_frame_source));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams* params) {
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::SINGLE_THREADED));
  layer_tree_host->InitializeSingleThreaded(
      single_thread_client, params->main_task_runner,
      std::move(params->external_begin_frame_source));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateRemoteServer(
    RemoteProtoChannel* remote_proto_channel,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(params->settings);
  DCHECK(remote_proto_channel);
  TRACE_EVENT0("cc.remote", "LayerTreeHost::CreateRemoteServer");

  // Using an external begin frame source is not supported on the server in
  // remote mode.
  DCHECK(!params->settings->use_external_begin_frame_source);
  DCHECK(!params->external_begin_frame_source);
  DCHECK(params->image_serialization_processor);

  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::REMOTE));
  layer_tree_host->InitializeRemoteServer(remote_proto_channel,
                                          params->main_task_runner);
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateRemoteClient(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(params->settings);
  DCHECK(remote_proto_channel);

  // Using an external begin frame source is not supported in remote mode.
  // TODO(khushalsagar): Add support for providing an external begin frame
  // source on the client LayerTreeHost. crbug/576962
  DCHECK(!params->settings->use_external_begin_frame_source);
  DCHECK(!params->external_begin_frame_source);
  DCHECK(params->image_serialization_processor);

  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::REMOTE));
  layer_tree_host->InitializeRemoteClient(
      remote_proto_channel, params->main_task_runner, impl_task_runner);
  return layer_tree_host;
}

LayerTreeHost::LayerTreeHost(InitParams* params, CompositorMode mode)
    : LayerTreeHost(
          params,
          mode,
          base::MakeUnique<LayerTree>(std::move(params->animation_host),
                                      this)) {}

LayerTreeHost::LayerTreeHost(InitParams* params,
                             CompositorMode mode,
                             std::unique_ptr<LayerTree> layer_tree)
    : micro_benchmark_controller_(this),
      layer_tree_(std::move(layer_tree)),
      next_ui_resource_id_(1),
      compositor_mode_(mode),
      client_(params->client),
      source_frame_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      output_surface_lost_(true),
      settings_(*params->settings),
      debug_state_(settings_.initial_debug_state),
      visible_(false),
      has_gpu_rasterization_trigger_(false),
      content_is_suitable_for_gpu_rasterization_(true),
      gpu_rasterization_histogram_recorded_(false),
      did_complete_scale_animation_(false),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      next_commit_forces_redraw_(false),
      shared_bitmap_manager_(params->shared_bitmap_manager),
      gpu_memory_buffer_manager_(params->gpu_memory_buffer_manager),
      task_graph_runner_(params->task_graph_runner),
      image_serialization_processor_(params->image_serialization_processor),
      surface_client_id_(0u),
      next_surface_sequence_(1u) {
  DCHECK(task_graph_runner_);
  DCHECK(layer_tree_);

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

void LayerTreeHost::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
  std::unique_ptr<ProxyMain> proxy_main =
      ProxyMain::CreateThreaded(this, task_runner_provider_.get());
  InitializeProxy(std::move(proxy_main),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);
  InitializeProxy(SingleThreadProxy::Create(this, single_thread_client,
                                            task_runner_provider_.get()),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializeRemoteServer(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);

  if (image_serialization_processor_) {
    engine_picture_cache_ =
        image_serialization_processor_->CreateEnginePictureCache();
  }

  // The LayerTreeHost on the server never requests the output surface since
  // it is only needed on the client. Since ProxyMain aborts commits if
  // output_surface_lost() is true, always assume we have the output surface
  // on the server.
  output_surface_lost_ = false;

  InitializeProxy(ProxyMain::CreateRemote(remote_proto_channel, this,
                                          task_runner_provider_.get()),
                  nullptr);
}

void LayerTreeHost::InitializeRemoteClient(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);

  if (image_serialization_processor_) {
    client_picture_cache_ =
        image_serialization_processor_->CreateClientPictureCache();
  }

  // For the remote mode, the RemoteChannelImpl implements the Proxy, which is
  // owned by the LayerTreeHost. The RemoteChannelImpl pipes requests which need
  // to handled locally, for instance the Output Surface creation to the
  // LayerTreeHost on the client, while the other requests are sent to the
  // RemoteChannelMain on the server which directs them to ProxyMain and the
  // remote server LayerTreeHost.
  InitializeProxy(base::MakeUnique<RemoteChannelImpl>(
                      this, remote_proto_channel, task_runner_provider_.get()),
                  nullptr);
}

void LayerTreeHost::InitializeForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider,
    std::unique_ptr<Proxy> proxy_for_testing,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ = std::move(task_runner_provider);

  InitializePictureCacheForTesting();

  InitializeProxy(std::move(proxy_for_testing),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializePictureCacheForTesting() {
  if (!image_serialization_processor_)
    return;

  // Initialize both engine and client cache to ensure serialization tests
  // with a single LayerTreeHost can work correctly.
  engine_picture_cache_ =
      image_serialization_processor_->CreateEnginePictureCache();
  client_picture_cache_ =
      image_serialization_processor_->CreateClientPictureCache();
}

void LayerTreeHost::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  DCHECK(!task_runner_provider_);
  task_runner_provider_ = std::move(task_runner_provider);
}

void LayerTreeHost::InitializeProxy(
    std::unique_ptr<Proxy> proxy,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");
  DCHECK(task_runner_provider_);

  proxy_ = std::move(proxy);
  proxy_->Start(std::move(external_begin_frame_source));

  layer_tree_->animation_host()->SetSupportsScrollAnimations(
      proxy_->SupportsImplScrolling());
}

LayerTreeHost::~LayerTreeHost() {
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  // Clear any references into the LayerTreeHost.
  layer_tree_.reset();

  DCHECK(swap_promise_monitor_.empty());

  BreakSwapPromises(SwapPromise::COMMIT_FAILS);

  if (proxy_) {
    DCHECK(task_runner_provider_->IsMainThread());
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }
}

void LayerTreeHost::WillBeginMainFrame() {
  devtools_instrumentation::WillBeginMainThreadFrame(id(),
                                                     source_frame_number());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrame(const BeginFrameArgs& args) {
  client_->BeginMainFrame(args);
}

void LayerTreeHost::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

void LayerTreeHost::RequestMainFrameUpdate() {
  client_->UpdateLayerTreeHost();
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(!IsRemoteServer());
  DCHECK(task_runner_provider_->IsImplThread());

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace &&
      frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() &&
      layer_tree_->root_layer()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        layer_tree_.get(), [](Layer* layer) { layer->DidBeginTracing(); });
  }

  LayerTreeImpl* sync_tree = host_impl->sync_tree();

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }

  sync_tree->set_source_frame_number(source_frame_number());

  if (layer_tree_->needs_full_tree_sync())
    TreeSynchronizer::SynchronizeTrees(layer_tree_->root_layer(), sync_tree);

  layer_tree_->PushPropertiesTo(sync_tree);

  sync_tree->PassSwapPromises(std::move(swap_promise_list_));
  swap_promise_list_.clear();

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  RecordGpuRasterizationHistogram();

  host_impl->SetViewportSize(layer_tree_->device_viewport_size());
  // TODO(senorblanco): Move this to LayerTree::PushPropertiesTo so that it
  // happens before GPU rasterization properties are set, since those trigger an
  // update of GPU rasterization status, which depends on the device scale
  // factor. (crbug.com/535700)
  sync_tree->SetDeviceScaleFactor(layer_tree_->device_scale_factor());
  host_impl->SetDebugState(debug_state_);

  if (!ui_resource_request_queue_.empty()) {
    sync_tree->set_ui_resource_request_queue(ui_resource_request_queue_);
    ui_resource_request_queue_.clear();
  }

  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");

    TreeSynchronizer::PushLayerProperties(layer_tree_.get(), sync_tree);

    // This must happen after synchronizing property trees and after push
    // properties, which updates property tree indices, but before animation
    // host pushes properties as animation host push properties can change
    // Animation::InEffect and we want the old InEffect value for updating
    // property tree scrolling and animation.
    sync_tree->UpdatePropertyTreeScrollingAndAnimationFromMainThread();

    TRACE_EVENT0("cc", "LayerTreeHost::AnimationHost::PushProperties");
    DCHECK(host_impl->animation_host());
    layer_tree_->animation_host()->PushPropertiesTo(
        host_impl->animation_host());
  }

  // This must happen after synchronizing property trees and after pushing
  // properties, which updates the clobber_active_value flag.
  sync_tree->UpdatePropertyTreeScrollOffset(layer_tree_->property_trees());

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
  layer_tree_->property_trees()->ResetAllChangeTracking();
}

void LayerTreeHost::WillCommit() {
  OnCommitForSwapPromises();
  client_->WillCommit();
}

void LayerTreeHost::UpdateHudLayer() {
}

void LayerTreeHost::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHost::SetOutputSurface(std::unique_ptr<OutputSurface> surface) {
  TRACE_EVENT0("cc", "LayerTreeHost::SetOutputSurface");
  DCHECK(output_surface_lost_);
  DCHECK(surface);

  DCHECK(!new_output_surface_);
  new_output_surface_ = std::move(surface);
  proxy_->SetOutputSurface(new_output_surface_.get());
}

std::unique_ptr<OutputSurface> LayerTreeHost::ReleaseOutputSurface() {
  DCHECK(!visible_);
  DCHECK(!output_surface_lost_);

  DidLoseOutputSurface();
  proxy_->ReleaseOutputSurface();
  return std::move(current_output_surface_);
}

void LayerTreeHost::RequestNewOutputSurface() {
  client_->RequestNewOutputSurface();
}

void LayerTreeHost::DidInitializeOutputSurface() {
  DCHECK(new_output_surface_);
  output_surface_lost_ = false;
  current_output_surface_ = std::move(new_output_surface_);
  client_->DidInitializeOutputSurface();
}

void LayerTreeHost::DidFailToInitializeOutputSurface() {
  DCHECK(output_surface_lost_);
  DCHECK(new_output_surface_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new output surface after failing to initialize the new one.
  current_output_surface_ = nullptr;
  new_output_surface_ = nullptr;
  client_->DidFailToInitializeOutputSurface();
}

std::unique_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(!IsRemoteServer());
  DCHECK(task_runner_provider_->IsImplThread());

  const bool supports_impl_scrolling = task_runner_provider_->HasImplThread();
  std::unique_ptr<AnimationHost> animation_host_impl =
      layer_tree_->animation_host()->CreateImplInstance(
          supports_impl_scrolling);

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, task_runner_provider_.get(),
      rendering_stats_instrumentation_.get(), shared_bitmap_manager_,
      gpu_memory_buffer_manager_, task_graph_runner_,
      std::move(animation_host_impl), id_);
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  shared_bitmap_manager_ = NULL;
  gpu_memory_buffer_manager_ = NULL;
  task_graph_runner_ = NULL;
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(task_runner_provider_->IsMainThread());

  if (output_surface_lost_)
    return;

  output_surface_lost_ = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsCommit() {
  proxy_->SetNeedsCommit();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsRedraw() {
  SetNeedsRedrawRect(gfx::Rect(layer_tree_->device_viewport_size()));
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
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
  proxy_->SetNeedsUpdateLayers();
}

void LayerTreeHost::SetAnimationEvents(
    std::unique_ptr<AnimationEvents> events) {
  DCHECK(task_runner_provider_->IsMainThread());
  layer_tree_->animation_host()->SetAnimationEvents(std::move(events));
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

void LayerTreeHost::ResetGpuRasterizationTracking() {
  content_is_suitable_for_gpu_rasterization_ = true;
  gpu_rasterization_histogram_recorded_ = false;
}

void LayerTreeHost::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1("cc",
                       "LayerTreeHost::SetHasGpuRasterizationTrigger",
                       TRACE_EVENT_SCOPE_THREAD,
                       "has_trigger",
                       has_gpu_rasterization_trigger_);
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  float page_scale = layer_tree_->page_scale_factor() * page_scale_delta;
  layer_tree_->SetPageScaleFromImplSide(page_scale);
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

void LayerTreeHost::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
}

void LayerTreeHost::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  if (output_surface_lost()) {
    proxy->RequestNewOutputSurface();
    // RequestNewOutputSurface could have synchronously created an output
    // surface, so check again before returning.
    if (output_surface_lost())
      return;
  }

  RequestMainFrameUpdate();
  UpdateLayers();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  proxy->CompositeImmediately(frame_begin_time);
}

bool LayerTreeHost::UpdateLayers() {
  DCHECK(!output_surface_lost_);
  if (!layer_tree_->root_layer())
    return false;
  DCHECK(!layer_tree_->root_layer()->parent());
  bool result = DoUpdateLayers(layer_tree_->root_layer());
  micro_benchmark_controller_.DidUpdateLayers();
  return result || next_commit_forces_redraw_;
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

void LayerTreeHost::RecordGpuRasterizationHistogram() {
  // Gpu rasterization is only supported for Renderer compositors.
  // Checking for IsSingleThreaded() to exclude Browser compositors.
  if (gpu_rasterization_histogram_recorded_ || IsSingleThreaded())
    return;

  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu whitelisting/backlisting into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        settings_.gpu_rasterization_enabled);
  if (settings_.gpu_rasterization_enabled) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationTriggered",
                          has_gpu_rasterization_trigger_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSuitableContent",
                          content_is_suitable_for_gpu_rasterization_);
    // Record how many pages actually get gpu rasterization when enabled.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationUsed",
                          (has_gpu_rasterization_trigger_ &&
                           content_is_suitable_for_gpu_rasterization_));
  }

  gpu_rasterization_histogram_recorded_ = true;
}

void LayerTreeHost::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::PreCalculateMetaInformation(layer_tree_->root_layer());
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      layer_tree_->root_layer(), layer_tree_->page_scale_layer(),
      layer_tree_->inner_viewport_scroll_layer(),
      layer_tree_->outer_viewport_scroll_layer(),
      layer_tree_->overscroll_elasticity_layer(),
      layer_tree_->elastic_overscroll(), layer_tree_->page_scale_factor(),
      layer_tree_->device_scale_factor(),
      gfx::Rect(layer_tree_->device_viewport_size()), identity_transform,
      layer_tree_->property_trees());
}

bool LayerTreeHost::DoUpdateLayers(Layer* root_layer) {
  TRACE_EVENT1("cc", "LayerTreeHost::DoUpdateLayers", "source_frame_number",
               source_frame_number());

  layer_tree_->UpdateHudLayer(debug_state_.ShowHudInfo());
  UpdateHudLayer();

  Layer* root_scroll =
      PropertyTreeBuilder::FindFirstScrollableLayer(root_layer);
  Layer* page_scale_layer = layer_tree_->page_scale_layer();
  if (!page_scale_layer && root_scroll)
    page_scale_layer = root_scroll->parent();

  if (layer_tree_->hud_layer()) {
    layer_tree_->hud_layer()->PrepareForCalculateDrawProperties(
        layer_tree_->device_viewport_size(),
        layer_tree_->device_scale_factor());
  }

  gfx::Transform identity_transform;
  LayerList update_layer_list;

  {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::BuildPropertyTrees");
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
                 "LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees");
    PropertyTreeBuilder::PreCalculateMetaInformation(root_layer);
    bool can_render_to_separate_surface = true;
    PropertyTrees* property_trees = layer_tree_->property_trees();
    if (!settings_.use_layer_lists) {
      // If use_layer_lists is set, then the property trees should have been
      // built by the client already.
      PropertyTreeBuilder::BuildPropertyTrees(
          root_layer, page_scale_layer,
          layer_tree_->inner_viewport_scroll_layer(),
          layer_tree_->outer_viewport_scroll_layer(),
          layer_tree_->overscroll_elasticity_layer(),
          layer_tree_->elastic_overscroll(), layer_tree_->page_scale_factor(),
          layer_tree_->device_scale_factor(),
          gfx::Rect(layer_tree_->device_viewport_size()), identity_transform,
          property_trees);
      TRACE_EVENT_INSTANT1("cc",
                           "LayerTreeHost::UpdateLayers_BuiltPropertyTrees",
                           TRACE_EVENT_SCOPE_THREAD, "property_trees",
                           property_trees->AsTracedValue());
    } else {
      TRACE_EVENT_INSTANT1("cc",
                           "LayerTreeHost::UpdateLayers_ReceivedPropertyTrees",
                           TRACE_EVENT_SCOPE_THREAD, "property_trees",
                           property_trees->AsTracedValue());
    }
    draw_property_utils::UpdatePropertyTrees(property_trees,
                                             can_render_to_separate_surface);
    draw_property_utils::FindLayersThatNeedUpdates(
        layer_tree_.get(), property_trees->transform_tree,
        property_trees->effect_tree, &update_layer_list);
  }

  for (const auto& layer : update_layer_list)
    layer->SavePaintProperties();

  bool content_is_suitable_for_gpu = true;
  bool did_paint_content = layer_tree_->UpdateLayers(
      update_layer_list, &content_is_suitable_for_gpu);

  if (content_is_suitable_for_gpu) {
    ++num_consecutive_frames_suitable_for_gpu_;
    if (num_consecutive_frames_suitable_for_gpu_ >=
        kNumFramesToConsiderBeforeGpuRasterization) {
      content_is_suitable_for_gpu_rasterization_ = true;
    }
  } else {
    num_consecutive_frames_suitable_for_gpu_ = 0;
    content_is_suitable_for_gpu_rasterization_ = false;
  }
  return did_paint_content;
}

void LayerTreeHost::ApplyViewportDeltas(ScrollAndScaleSet* info) {
  gfx::Vector2dF inner_viewport_scroll_delta;
  if (info->inner_viewport_scroll.layer_id != Layer::INVALID_ID)
    inner_viewport_scroll_delta = info->inner_viewport_scroll.scroll_delta;

  if (inner_viewport_scroll_delta.IsZero() && info->page_scale_delta == 1.f &&
      info->elastic_overscroll_delta.IsZero() && !info->top_controls_delta)
    return;

  // Preemptively apply the scroll offset and scale delta here before sending
  // it to the client.  If the client comes back and sets it to the same
  // value, then the layer can early out without needing a full commit.
  if (layer_tree_->inner_viewport_scroll_layer()) {
    layer_tree_->inner_viewport_scroll_layer()->SetScrollOffsetFromImplSide(
        gfx::ScrollOffsetWithDelta(
            layer_tree_->inner_viewport_scroll_layer()->scroll_offset(),
            inner_viewport_scroll_delta));
  }

  ApplyPageScaleDeltaFromImplSide(info->page_scale_delta);
  layer_tree_->SetElasticOverscrollFromImplSide(
      layer_tree_->elastic_overscroll() + info->elastic_overscroll_delta);
  // TODO(ccameron): pass the elastic overscroll here so that input events
  // may be translated appropriately.
  client_->ApplyViewportDeltas(inner_viewport_scroll_delta, gfx::Vector2dF(),
                               info->elastic_overscroll_delta,
                               info->page_scale_delta,
                               info->top_controls_delta);
  SetNeedsUpdateLayers();
}

void LayerTreeHost::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  for (auto& swap_promise : info->swap_promises) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark",
                           "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Main thread scroll update");
    QueueSwapPromise(std::move(swap_promise));
  }

  if (layer_tree_->root_layer()) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = layer_tree_->LayerById(info->scrolls[i].layer_id);
      if (!layer)
        continue;
      layer->SetScrollOffsetFromImplSide(gfx::ScrollOffsetWithDelta(
          layer->scroll_offset(), info->scrolls[i].scroll_delta));
      SetNeedsUpdateLayers();
    }
  }

  // This needs to happen after scroll deltas have been sent to prevent top
  // controls from clamping the layout viewport both on the compositor and
  // on the main thread.
  ApplyViewportDeltas(info);
}

void LayerTreeHost::UpdateTopControlsState(TopControlsState constraints,
                                           TopControlsState current,
                                           bool animate) {
  // Top controls are only used in threaded or remote mode.
  DCHECK(IsThreaded() || IsRemoteServer());
  proxy_->UpdateTopControlsState(constraints, current, animate);
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  AnimationHost* animation_host = layer_tree_->animation_host();
  std::unique_ptr<AnimationEvents> events = animation_host->CreateEvents();

  if (animation_host->AnimateLayers(monotonic_time))
    animation_host->UpdateAnimationState(true, events.get());

  if (!events->events_.empty())
    layer_tree_->property_trees()->needs_rebuild = true;
}

UIResourceId LayerTreeHost::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(ui_resource_client_map_.find(next_id) ==
         ui_resource_client_map_.end());

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, next_id,
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

  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_DELETE, uid);
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
    UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, uid,
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

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  return micro_benchmark_controller_.ScheduleRun(benchmark_name,
                                                 std::move(value), callback);
}

bool LayerTreeHost::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, std::move(value));
}

void LayerTreeHost::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHost::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHost::NotifySwapPromiseMonitorsOfSetNeedsCommit() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsCommitOnMain();
}

void LayerTreeHost::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeHost::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  swap_promise_list_.clear();
}

std::vector<std::unique_ptr<SwapPromise>> LayerTreeHost::TakeSwapPromises() {
  std::vector<std::unique_ptr<SwapPromise>> to_return;
  to_return.swap(swap_promise_list_);
  return to_return;
}

void LayerTreeHost::OnCommitForSwapPromises() {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->OnCommit();
}

void LayerTreeHost::set_surface_client_id(uint32_t client_id) {
  surface_client_id_ = client_id;
}

SurfaceSequence LayerTreeHost::CreateSurfaceSequence() {
  return SurfaceSequence(surface_client_id_, next_surface_sequence_++);
}

void LayerTreeHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  proxy_->SetMutator(std::move(mutator));
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

bool LayerTreeHost::IsRemoteServer() const {
  // The LayerTreeHost on the server does not have an impl task runner.
  return compositor_mode_ == CompositorMode::REMOTE &&
         !task_runner_provider_->HasImplThread();
}

bool LayerTreeHost::IsRemoteClient() const {
  return compositor_mode_ == CompositorMode::REMOTE &&
         task_runner_provider_->HasImplThread();
}

void LayerTreeHost::ToProtobufForCommit(
    proto::LayerTreeHost* proto,
    std::vector<std::unique_ptr<SwapPromise>>* swap_promises) {
  DCHECK(engine_picture_cache_);
  // Not all fields are serialized, as they are either not needed for a commit,
  // or implementation isn't ready yet.
  // Unsupported items:
  // - animations
  // - UI resources
  // - instrumentation of stats
  // - histograms
  // Skipped items:
  // - SwapPromise as they are mostly used for perf measurements.
  // - The bitmap and GPU memory related items.
  // Other notes:
  // - The output surfaces are only valid on the client-side so they are
  //   therefore not serialized.
  // - LayerTreeSettings are needed only during construction of the
  //   LayerTreeHost, so they are serialized outside of the LayerTreeHost
  //   serialization.
  // - The |visible_| flag will be controlled from the client separately and
  //   will need special handling outside of the serialization of the
  //   LayerTreeHost.
  // TODO(nyquist): Figure out how to support animations. See crbug.com/570376.
  TRACE_EVENT0("cc.remote", "LayerTreeHost::ToProtobufForCommit");
  swap_promises->swap(swap_promise_list_);
  DCHECK(swap_promise_list_.empty());

  proto->set_source_frame_number(source_frame_number_);

  // Serialize the LayerTree before serializing the properties. During layer
  // property serialization, we clear the list |layer_that_should_properties_|
  // from the LayerTree.
  layer_tree_->ToProtobuf(proto->mutable_layer_tree());

  LayerProtoConverter::SerializeLayerProperties(this,
                                                proto->mutable_layer_updates());

  std::vector<PictureData> pictures =
      engine_picture_cache_->CalculateCacheUpdateAndFlush();
  proto::PictureDataVectorToSkPicturesProto(pictures,
                                            proto->mutable_pictures());

  debug_state_.ToProtobuf(proto->mutable_debug_state());
  proto->set_has_gpu_rasterization_trigger(has_gpu_rasterization_trigger_);
  proto->set_content_is_suitable_for_gpu_rasterization(
      content_is_suitable_for_gpu_rasterization_);
  proto->set_id(id_);
  proto->set_next_commit_forces_redraw(next_commit_forces_redraw_);

  proto->set_surface_client_id(surface_client_id_);
  proto->set_next_surface_sequence(next_surface_sequence_);

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      "cc.remote", "LayerTreeHostProto", source_frame_number_,
      ComputeLayerTreeHostProtoSizeSplitAsValue(proto));
}

void LayerTreeHost::FromProtobufForCommit(const proto::LayerTreeHost& proto) {
  DCHECK(client_picture_cache_);
  source_frame_number_ = proto.source_frame_number();

  layer_tree_->FromProtobuf(proto.layer_tree());

  // Ensure ClientPictureCache contains all the necessary SkPictures before
  // deserializing the properties.
  proto::SkPictures proto_pictures = proto.pictures();
  std::vector<PictureData> pictures =
      SkPicturesProtoToPictureDataVector(proto_pictures);
  client_picture_cache_->ApplyCacheUpdate(pictures);

  LayerProtoConverter::DeserializeLayerProperties(layer_tree_->root_layer(),
                                                  proto.layer_updates());

  // The deserialization is finished, so now clear the cache.
  client_picture_cache_->Flush();

  debug_state_.FromProtobuf(proto.debug_state());
  has_gpu_rasterization_trigger_ = proto.has_gpu_rasterization_trigger();
  content_is_suitable_for_gpu_rasterization_ =
      proto.content_is_suitable_for_gpu_rasterization();
  id_ = proto.id();
  next_commit_forces_redraw_ = proto.next_commit_forces_redraw();

  surface_client_id_ = proto.surface_client_id();
  next_surface_sequence_ = proto.next_surface_sequence();
}

AnimationHost* LayerTreeHost::animation_host() const {
  return layer_tree_->animation_host();
}

}  // namespace cc
