// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IN_PROCESS_H_
#define CC_TREES_LAYER_TREE_HOST_IN_PROCESS_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/target_property.h"
#include "cc/base/cc_export.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/resource_format.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {
class GpuMemoryBufferManager;
}  // namespace gpu

namespace cc {
class AnimationHost;
class AnimationEvents;
class BeginFrameSource;
class ClientPictureCache;
class EnginePictureCache;
class HeadsUpDisplayLayer;
class ImageSerializationProcessor;
class Layer;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class LayerTreeHostSingleThreadClient;
class LayerTreeMutator;
class PropertyTrees;
class Region;
class RemoteProtoChannel;
class RenderingStatsInstrumentation;
class ResourceProvider;
class ResourceUpdateQueue;
class SharedBitmapManager;
class TaskGraphRunner;
struct PendingPageScaleAnimation;
struct RenderingStats;
struct ScrollAndScaleSet;

namespace proto {
class LayerTreeHost;
}

class CC_EXPORT LayerTreeHostInProcess : public LayerTreeHost {
 public:
  // TODO(sad): InitParams should be a movable type so that it can be
  // std::move()d to the Create* functions.
  struct CC_EXPORT InitParams {
    LayerTreeHostClient* client = nullptr;
    SharedBitmapManager* shared_bitmap_manager = nullptr;
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr;
    TaskGraphRunner* task_graph_runner = nullptr;
    LayerTreeSettings const* settings = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
    ImageSerializationProcessor* image_serialization_processor = nullptr;
    std::unique_ptr<AnimationHost> animation_host;

    InitParams();
    ~InitParams();
  };

  // The SharedBitmapManager will be used on the compositor thread.
  static std::unique_ptr<LayerTreeHostInProcess> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  static std::unique_ptr<LayerTreeHostInProcess> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams* params);

  static std::unique_ptr<LayerTreeHostInProcess> CreateRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      InitParams* params);

  // The lifetime of this LayerTreeHostInProcess is tied to the lifetime of the
  // remote server LayerTreeHostInProcess. It should be created on receiving
  // CompositorMessageToImpl::InitializeImpl message and destroyed on receiving
  // a CompositorMessageToImpl::CloseImpl message from the server. This ensures
  // that the client will not send any compositor messages once the
  // LayerTreeHostInProcess on the server is destroyed.
  static std::unique_ptr<LayerTreeHostInProcess> CreateRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  ~LayerTreeHostInProcess() override;

  // LayerTreeHost implementation.
  int GetId() const override;
  int SourceFrameNumber() const override;
  LayerTree* GetLayerTree() override;
  const LayerTree* GetLayerTree() const override;
  UIResourceManager* GetUIResourceManager() const override;
  TaskRunnerProvider* GetTaskRunnerProvider() const override;
  const LayerTreeSettings& GetSettings() const override;
  void SetFrameSinkId(const FrameSinkId& frame_sink_id) override;
  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise) override;
  SwapPromiseManager* GetSwapPromiseManager() override;
  void SetHasGpuRasterizationTrigger(bool has_trigger) override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void SetCompositorFrameSink(
      std::unique_ptr<CompositorFrameSink> compositor_frame_sink) override;
  std::unique_ptr<CompositorFrameSink> ReleaseCompositorFrameSink() override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRecalculateRasterScales() override;
  bool BeginMainFrameRequested() const override;
  bool CommitRequested() const override;
  void SetDeferCommits(bool defer_commits) override;
  void LayoutAndUpdateLayers() override;
  void Composite(base::TimeTicks frame_begin_time) override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void SetNextCommitForcesRedraw() override;
  void NotifyInputThrottledUntilCommit() override;
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate) override;
  const base::WeakPtr<InputHandler>& GetInputHandler() const override;
  void DidStopFlinging() override;
  void SetDebugState(const LayerTreeDebugState& debug_state) override;
  const LayerTreeDebugState& GetDebugState() const override;
  int ScheduleMicroBenchmark(
      const std::string& benchmark_name,
      std::unique_ptr<base::Value> value,
      const MicroBenchmark::DoneCallback& callback) override;
  bool SendMessageToMicroBenchmark(int id,
                                   std::unique_ptr<base::Value> value) override;
  SurfaceSequenceGenerator* GetSurfaceSequenceGenerator() override;
  void SetNextCommitWaitsForActivation() override;
  void ResetGpuRasterizationTracking() override;

  // LayerTreeHostInProcess interface to Proxy.
  void WillBeginMainFrame();
  void DidBeginMainFrame();
  void BeginMainFrame(const BeginFrameArgs& args);
  void BeginMainFrameNotExpectedSoon();
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void RequestMainFrameUpdate();
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  void RequestNewCompositorFrameSink();
  void DidInitializeCompositorFrameSink();
  void DidFailToInitializeCompositorFrameSink();
  virtual std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseCompositorFrameSink();
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidReceiveCompositorFrameAck() {
    client_->DidReceiveCompositorFrameAck();
  }
  bool UpdateLayers();
  // Called when the compositor completed page scale animation.
  void DidCompletePageScaleAnimation();
  void ApplyScrollAndScale(ScrollAndScaleSet* info);

  LayerTreeHostClient* client() { return client_; }

  bool gpu_rasterization_histogram_recorded() const {
    return gpu_rasterization_histogram_recorded_;
  }

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  void SetAnimationEvents(std::unique_ptr<AnimationEvents> events);

  bool has_gpu_rasterization_trigger() const {
    return has_gpu_rasterization_trigger_;
  }

  Proxy* proxy() const { return proxy_.get(); }

  // Serializes the parts of this LayerTreeHostInProcess that is needed for a
  // commit to a protobuf message. Not all members are serialized as they are
  // not helpful for remote usage.
  // The |swap_promise_list_| is transferred to the serializer in
  // |swap_promises|.
  void ToProtobufForCommit(
      proto::LayerTreeHost* proto,
      std::vector<std::unique_ptr<SwapPromise>>* swap_promises);

  // Deserializes the protobuf into this LayerTreeHostInProcess before a commit.
  // The expected input is a serialized remote LayerTreeHost. After
  // deserializing the protobuf, the normal commit-flow should continue.
  void FromProtobufForCommit(const proto::LayerTreeHost& proto);

  bool IsSingleThreaded() const;
  bool IsThreaded() const;
  bool IsRemoteServer() const;
  bool IsRemoteClient() const;

  ImageSerializationProcessor* image_serialization_processor() const {
    return image_serialization_processor_;
  }

  EnginePictureCache* engine_picture_cache() const {
    return engine_picture_cache_ ? engine_picture_cache_.get() : nullptr;
  }

  ClientPictureCache* client_picture_cache() const {
    return client_picture_cache_ ? client_picture_cache_.get() : nullptr;
  }

 protected:
  // Allow tests to inject the LayerTree.
  LayerTreeHostInProcess(InitParams* params,
                         CompositorMode mode,
                         std::unique_ptr<LayerTree> layer_tree);
  LayerTreeHostInProcess(InitParams* params, CompositorMode mode);

  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing);
  void InitializePictureCacheForTesting();
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);
  void SetUIResourceManagerForTesting(
      std::unique_ptr<UIResourceManager> ui_resource_manager);

  // shared_bitmap_manager(), gpu_memory_buffer_manager(), and
  // task_graph_runner() return valid values only until the LayerTreeHostImpl is
  // created in CreateLayerTreeHostImpl().
  SharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_;
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }
  TaskGraphRunner* task_graph_runner() const { return task_graph_runner_; }

  void OnCommitForSwapPromises();

  void RecordGpuRasterizationHistogram();

  MicroBenchmarkController micro_benchmark_controller_;

  std::unique_ptr<LayerTree> layer_tree_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;

 private:
  friend class LayerTreeHostSerializationTest;

  // This is the number of consecutive frames in which we want the content to be
  // suitable for GPU rasterization before re-enabling it.
  enum { kNumFramesToConsiderBeforeGpuRasterization = 60 };

  void ApplyViewportDeltas(ScrollAndScaleSet* info);
  void ApplyPageScaleDeltaFromImplSide(float page_scale_delta);
  void InitializeProxy(std::unique_ptr<Proxy> proxy);

  bool DoUpdateLayers(Layer* root_layer);
  void UpdateHudLayer();

  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);

  void CalculateLCDTextMetricsCallback(Layer* layer);

  void SetPropertyTreesNeedRebuild();

  const CompositorMode compositor_mode_;

  std::unique_ptr<UIResourceManager> ui_resource_manager_;

  LayerTreeHostClient* client_;
  std::unique_ptr<Proxy> proxy_;
  std::unique_ptr<TaskRunnerProvider> task_runner_provider_;

  int source_frame_number_;
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  SwapPromiseManager swap_promise_manager_;

  // |current_compositor_frame_sink_| can't be updated until we've successfully
  // initialized a new CompositorFrameSink. |new_compositor_frame_sink_|
  // contains the new CompositorFrameSink that is currently being initialized.
  // If initialization is successful then |new_compositor_frame_sink_| replaces
  // |current_compositor_frame_sink_|.
  std::unique_ptr<CompositorFrameSink> new_compositor_frame_sink_;
  std::unique_ptr<CompositorFrameSink> current_compositor_frame_sink_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  bool visible_;

  bool has_gpu_rasterization_trigger_;
  bool content_is_suitable_for_gpu_rasterization_;
  bool gpu_rasterization_histogram_recorded_;

  // If set, then page scale animation has completed, but the client hasn't been
  // notified about it yet.
  bool did_complete_scale_animation_;

  int id_;
  bool next_commit_forces_redraw_ = false;
  bool next_commit_forces_recalculate_raster_scales_ = false;

  SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  TaskGraphRunner* task_graph_runner_;

  ImageSerializationProcessor* image_serialization_processor_;
  std::unique_ptr<EnginePictureCache> engine_picture_cache_;
  std::unique_ptr<ClientPictureCache> client_picture_cache_;

  SurfaceSequenceGenerator surface_sequence_generator_;
  uint32_t num_consecutive_frames_suitable_for_gpu_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHostInProcess);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IN_PROCESS_H_
