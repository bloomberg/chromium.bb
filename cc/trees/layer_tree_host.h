// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

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
#include "cc/input/event_listener_properties.h"
#include "cc/input/input_handler.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/scrollbar.h"
#include "cc/input/top_controls_state.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/output/output_surface.h"
#include "cc/output/swap_promise.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_interface.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise_monitor.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {

class AnimationEvents;
class AnimationHost;
class BeginFrameSource;
class ClientPictureCache;
class EnginePictureCache;
class HeadsUpDisplayLayer;
class ImageSerializationProcessor;
class Layer;
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
class TopControlsManager;
class UIResourceRequest;
struct PendingPageScaleAnimation;
struct RenderingStats;
struct ScrollAndScaleSet;

namespace proto {
class LayerTreeHost;
}

// This class is being refactored to an interface, see LayerTreeHostInterface,
// which is the API for the cc embedder. When adding new code to this class,
// consider the following:
// 1) If its state/data that gets pushed to the LayerTreeImpl during commit, add
//    it to the LayerTree.
// 2) If it's a call from the embedder, add it to the LayerTreeHostInterface.
// 3) If it's a call from any of the internal cc classes, i.e., LayerTree or
//    PropertyTreeBuilder, etc., add it to the LayerTreeHostInterface.
// 4) If it's a notification from the impl thread or a call from Proxy, add it
//    to this class.
// This class will be renamed to LayerTreeHostInProcess and will be the
// LayerTreeHost implementation for when the impl thread for the compositor runs
// in the same process, and thus uses a Proxy.
class CC_EXPORT LayerTreeHost : public LayerTreeHostInterface {
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
    std::unique_ptr<BeginFrameSource> external_begin_frame_source;
    ImageSerializationProcessor* image_serialization_processor = nullptr;
    std::unique_ptr<AnimationHost> animation_host;

    InitParams();
    ~InitParams();
  };

  // The SharedBitmapManager will be used on the compositor thread.
  static std::unique_ptr<LayerTreeHostInterface> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  static std::unique_ptr<LayerTreeHost> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams* params);

  static std::unique_ptr<LayerTreeHostInterface> CreateRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      InitParams* params);

  // The lifetime of this LayerTreeHost is tied to the lifetime of the remote
  // server LayerTreeHost. It should be created on receiving
  // CompositorMessageToImpl::InitializeImpl message and destroyed on receiving
  // a CompositorMessageToImpl::CloseImpl message from the server. This ensures
  // that the client will not send any compositor messages once the
  // LayerTreeHost on the server is destroyed.
  static std::unique_ptr<LayerTreeHostInterface> CreateRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  ~LayerTreeHost() override;

  // LayerTreeHostInterface implementation.
  int GetId() const override;
  int SourceFrameNumber() const override;
  LayerTree* GetLayerTree() override;
  const LayerTree* GetLayerTree() const override;
  TaskRunnerProvider* GetTaskRunnerProvider() const override;
  const LayerTreeSettings& GetSettings() const override;
  void SetSurfaceClientId(uint32_t client_id) override;
  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator) override;
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise) override;
  void SetHasGpuRasterizationTrigger(bool has_trigger) override;
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void SetOutputSurface(std::unique_ptr<OutputSurface> output_surface) override;
  std::unique_ptr<OutputSurface> ReleaseOutputSurface() override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  bool BeginMainFrameRequested() const override;
  bool CommitRequested() const override;
  void SetDeferCommits(bool defer_commits) override;
  void LayoutAndUpdateLayers() override;
  void Composite(base::TimeTicks frame_begin_time) override;
  void SetNeedsRedraw() override;
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override;
  void SetNextCommitForcesRedraw() override;
  void NotifyInputThrottledUntilCommit() override;
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
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
  void InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) override;
  void RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) override;

  // LayerTreeHost interface to Proxy.
  void WillBeginMainFrame();
  void DidBeginMainFrame();
  void BeginMainFrame(const BeginFrameArgs& args);
  void BeginMainFrameNotExpectedSoon();
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void RequestMainFrameUpdate();
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  void RequestNewOutputSurface();
  void DidInitializeOutputSurface();
  void DidFailToInitializeOutputSurface();
  virtual std::unique_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseOutputSurface();
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidCompleteSwapBuffers() { client_->DidCompleteSwapBuffers(); }
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

  void SetNextCommitWaitsForActivation();

  void SetAnimationEvents(std::unique_ptr<AnimationEvents> events);

  bool has_gpu_rasterization_trigger() const {
    return has_gpu_rasterization_trigger_;
  }

  Proxy* proxy() const { return proxy_.get(); }

  // CreateUIResource creates a resource given a bitmap.  The bitmap is
  // generated via an interface function, which is called when initializing the
  // resource and when the resource has been lost (due to lost context).  The
  // parameter of the interface is a single boolean, which indicates whether the
  // resource has been lost or not.  CreateUIResource returns an Id of the
  // resource, which is always positive.
  virtual UIResourceId CreateUIResource(UIResourceClient* client);
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId id);
  // Put the recreation of all UI resources into the resource queue after they
  // were evicted on the impl thread.
  void RecreateUIResources();

  virtual gfx::Size GetUIResourceSize(UIResourceId id) const;

  void BreakSwapPromises(SwapPromise::DidNotSwapReason reason);
  std::vector<std::unique_ptr<SwapPromise>> TakeSwapPromises();

  size_t num_queued_swap_promises() const { return swap_promise_list_.size(); }

  SurfaceSequence CreateSurfaceSequence();

  // Serializes the parts of this LayerTreeHost that is needed for a commit to a
  // protobuf message. Not all members are serialized as they are not helpful
  // for remote usage.
  // The |swap_promise_list_| is transferred to the serializer in
  // |swap_promises|.
  void ToProtobufForCommit(
      proto::LayerTreeHost* proto,
      std::vector<std::unique_ptr<SwapPromise>>* swap_promises);

  // Deserializes the protobuf into this LayerTreeHost before a commit. The
  // expected input is a serialized remote LayerTreeHost. After deserializing
  // the protobuf, the normal commit-flow should continue.
  void FromProtobufForCommit(const proto::LayerTreeHost& proto);

  bool IsSingleThreaded() const;
  bool IsThreaded() const;
  bool IsRemoteServer() const;
  bool IsRemoteClient() const;
  void BuildPropertyTreesForTesting();

  ImageSerializationProcessor* image_serialization_processor() const {
    return image_serialization_processor_;
  }

  EnginePictureCache* engine_picture_cache() const {
    return engine_picture_cache_ ? engine_picture_cache_.get() : nullptr;
  }

  ClientPictureCache* client_picture_cache() const {
    return client_picture_cache_ ? client_picture_cache_.get() : nullptr;
  }

  void ResetGpuRasterizationTracking();

 protected:
  // Allow tests to inject the LayerTree.
  LayerTreeHost(InitParams* params,
                CompositorMode mode,
                std::unique_ptr<LayerTree> layer_tree);
  LayerTreeHost(InitParams* params, CompositorMode mode);

  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializeRemoteServer(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeRemoteClient(
      RemoteProtoChannel* remote_proto_channel,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);
  void InitializePictureCacheForTesting();
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);

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
  void InitializeProxy(
      std::unique_ptr<Proxy> proxy,
      std::unique_ptr<BeginFrameSource> external_begin_frame_source);

  bool DoUpdateLayers(Layer* root_layer);
  void UpdateHudLayer();

  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);

  struct UIResourceClientData {
    UIResourceClient* client;
    gfx::Size size;
  };

  using UIResourceClientMap =
      std::unordered_map<UIResourceId, UIResourceClientData>;
  UIResourceClientMap ui_resource_client_map_;
  int next_ui_resource_id_;

  using UIResourceRequestQueue = std::vector<UIResourceRequest>;
  UIResourceRequestQueue ui_resource_request_queue_;

  void CalculateLCDTextMetricsCallback(Layer* layer);

  void NotifySwapPromiseMonitorsOfSetNeedsCommit();

  void SetPropertyTreesNeedRebuild();

  const CompositorMode compositor_mode_;

  LayerTreeHostClient* client_;
  std::unique_ptr<Proxy> proxy_;
  std::unique_ptr<TaskRunnerProvider> task_runner_provider_;

  int source_frame_number_;
  std::unique_ptr<RenderingStatsInstrumentation>
      rendering_stats_instrumentation_;

  // |current_output_surface_| can't be updated until we've successfully
  // initialized a new output surface. |new_output_surface_| contains the
  // new output surface that is currently being initialized. If initialization
  // is successful then |new_output_surface_| replaces
  // |current_output_surface_|.
  std::unique_ptr<OutputSurface> new_output_surface_;
  std::unique_ptr<OutputSurface> current_output_surface_;

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
  bool next_commit_forces_redraw_;

  SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  TaskGraphRunner* task_graph_runner_;

  ImageSerializationProcessor* image_serialization_processor_;
  std::unique_ptr<EnginePictureCache> engine_picture_cache_;
  std::unique_ptr<ClientPictureCache> client_picture_cache_;

  std::vector<std::unique_ptr<SwapPromise>> swap_promise_list_;
  std::set<SwapPromiseMonitor*> swap_promise_monitor_;

  uint32_t surface_client_id_;
  uint32_t next_surface_sequence_;
  uint32_t num_consecutive_frames_suitable_for_gpu_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
