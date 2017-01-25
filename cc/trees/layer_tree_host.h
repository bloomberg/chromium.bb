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
#include "cc/surfaces/surface_reference_owner.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "cc/trees/compositor_mode.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class MutatorEvents;
class Layer;
class LayerTreeHostClient;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class LayerTreeHostSingleThreadClient;
class LayerTreeMutator;
class MutatorHost;
class RenderingStatsInstrumentation;
class TaskGraphRunner;
class UIResourceManager;
struct RenderingStats;
struct ScrollAndScaleSet;

class CC_EXPORT LayerTreeHost
    : public NON_EXPORTED_BASE(SurfaceReferenceOwner) {
 public:
  // TODO(sad): InitParams should be a movable type so that it can be
  // std::move()d to the Create* functions.
  struct CC_EXPORT InitParams {
    LayerTreeHostClient* client = nullptr;
    TaskGraphRunner* task_graph_runner = nullptr;
    LayerTreeSettings const* settings = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner;
    MutatorHost* mutator_host = nullptr;
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner;

    InitParams();
    ~InitParams();
  };

  static std::unique_ptr<LayerTreeHost> CreateThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      InitParams* params);

  static std::unique_ptr<LayerTreeHost> CreateSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      InitParams* params);

  ~LayerTreeHost() override;

  // Returns the process global unique identifier for this LayerTreeHost.
  int GetId() const;

  // The current source frame number. This is incremented for each main frame
  // update(commit) pushed to the compositor thread.
  int SourceFrameNumber() const;

  // Returns the LayerTree that holds the main frame state pushed to the
  // LayerTreeImpl on commit.
  LayerTree* GetLayerTree();
  const LayerTree* GetLayerTree() const;

  // Returns the UIResourceManager used to create UIResources for
  // UIResourceLayers pushed to the LayerTree.
  UIResourceManager* GetUIResourceManager() const;

  // Returns the TaskRunnerProvider used to access the main and compositor
  // thread task runners.
  TaskRunnerProvider* GetTaskRunnerProvider() const;

  // Returns the settings used by this host.
  const LayerTreeSettings& GetSettings() const;

  // Sets the client id used to generate the SurfaceId that uniquely identifies
  // the Surfaces produced by this compositor.
  void SetFrameSinkId(const FrameSinkId& frame_sink_id);

  // Sets the LayerTreeMutator interface used to directly mutate the compositor
  // state on the compositor thread. (Compositor-Worker)
  void SetLayerTreeMutator(std::unique_ptr<LayerTreeMutator> mutator);

  // Call this function when you expect there to be a swap buffer.
  // See swap_promise.h for how to use SwapPromise.
  void QueueSwapPromise(std::unique_ptr<SwapPromise> swap_promise);

  // Returns the SwapPromiseManager used to create SwapPromiseMonitors for this
  // host.
  SwapPromiseManager* GetSwapPromiseManager();

  // Sets whether the content is suitable to use Gpu Rasterization.
  void SetHasGpuRasterizationTrigger(bool has_trigger);

  // Visibility and CompositorFrameSink -------------------------------

  void SetVisible(bool visible);
  bool IsVisible() const;

  // Called in response to an CompositorFrameSink request made to the client
  // using LayerTreeHostClient::RequestNewCompositorFrameSink. The client will
  // be informed of the CompositorFrameSink initialization status using
  // DidInitializaCompositorFrameSink or DidFailToInitializeCompositorFrameSink.
  // The request is completed when the host successfully initializes an
  // CompositorFrameSink.
  void SetCompositorFrameSink(
      std::unique_ptr<CompositorFrameSink> compositor_frame_sink);

  // Forces the host to immediately release all references to the
  // CompositorFrameSink, if any. Can be safely called any time.
  std::unique_ptr<CompositorFrameSink> ReleaseCompositorFrameSink();

  // Frame Scheduling (main and compositor frames) requests -------

  // Requests a main frame update even if no content has changed. This is used,
  // for instance in the case of RequestAnimationFrame from blink to ensure the
  // main frame update is run on the next tick without pre-emptively forcing a
  // full commit synchronization or layer updates.
  void SetNeedsAnimate();

  // Requests a main frame update and also ensure that the host pulls layer
  // updates from the client, even if no content might have changed, without
  // forcing a full commit synchronization.
  virtual void SetNeedsUpdateLayers();

  // Requests that the next main frame update performs a full commit
  // synchronization.
  virtual void SetNeedsCommit();

  // Requests that the next frame re-chooses crisp raster scales for all layers.
  void SetNeedsRecalculateRasterScales();

  // Returns true if a main frame with commit synchronization has been
  // requested.
  bool CommitRequested() const;

  // Enables/disables the compositor from requesting main frame updates from the
  // client.
  void SetDeferCommits(bool defer_commits);

  // Synchronously performs a main frame update and layer updates. Used only in
  // single threaded mode when the compositor's internal scheduling is disabled.
  void LayoutAndUpdateLayers();

  // Synchronously performs a complete main frame update, commit and compositor
  // frame. Used only in single threaded mode when the compositor's internal
  // scheduling is disabled.
  void Composite(base::TimeTicks frame_begin_time);

  // Requests a redraw (compositor frame) for the given rect.
  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);

  // Requests a main frame (including layer updates) and ensures that this main
  // frame results in a redraw for the complete viewport when producing the
  // CompositorFrame.
  void SetNextCommitForcesRedraw();

  // Input Handling ---------------------------------------------

  // Notifies the compositor that input from the browser is being throttled till
  // the next commit. The compositor will prioritize activation of the pending
  // tree so a commit can be performed.
  void NotifyInputThrottledUntilCommit();

  // Sets the state of the browser controls. (Used for URL bar animations on
  // android).
  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate);

  // Returns a reference to the InputHandler used to respond to input events on
  // the compositor thread.
  const base::WeakPtr<InputHandler>& GetInputHandler() const;

  // Informs the compositor that an active fling gesture being processed on the
  // main thread has been finished.
  void DidStopFlinging();

  // Debugging and benchmarks ---------------------------------
  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& GetDebugState() const;

  // Returns the id of the benchmark on success, 0 otherwise.
  int ScheduleMicroBenchmark(const std::string& benchmark_name,
                             std::unique_ptr<base::Value> value,
                             const MicroBenchmark::DoneCallback& callback);

  // Returns true if the message was successfully delivered and handled.
  bool SendMessageToMicroBenchmark(int id, std::unique_ptr<base::Value> value);

  // When the main thread informs the impl thread that it is ready to commit,
  // generally it would remain blocked till the main thread state is copied to
  // the pending tree. Calling this would ensure that the main thread remains
  // blocked till the pending tree is activated.
  void SetNextCommitWaitsForActivation();

  // The LayerTreeHost tracks whether the content is suitable for Gpu raster.
  // Calling this will reset it back to not suitable state.
  void ResetGpuRasterizationTracking();

  // SurfaceReferenceOwner implementation.
  SurfaceSequenceGenerator* GetSurfaceSequenceGenerator() override;

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

  void SetAnimationEvents(std::unique_ptr<MutatorEvents> events);

  bool has_gpu_rasterization_trigger() const {
    return has_gpu_rasterization_trigger_;
  }

  Proxy* proxy() const { return proxy_.get(); }

  bool IsSingleThreaded() const;
  bool IsThreaded() const;

 protected:
  // Allow tests to inject the LayerTree.
  LayerTreeHost(InitParams* params,
                CompositorMode mode,
                std::unique_ptr<LayerTree> layer_tree);
  LayerTreeHost(InitParams* params, CompositorMode mode);

  void InitializeThreaded(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  void InitializeSingleThreaded(
      LayerTreeHostSingleThreadClient* single_thread_client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  void InitializeForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider,
      std::unique_ptr<Proxy> proxy_for_testing);
  void SetTaskRunnerProviderForTesting(
      std::unique_ptr<TaskRunnerProvider> task_runner_provider);
  void SetUIResourceManagerForTesting(
      std::unique_ptr<UIResourceManager> ui_resource_manager);

  // task_graph_runner() returns a valid value only until the LayerTreeHostImpl
  // is created in CreateLayerTreeHostImpl().
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
  // Track when we're inside a main frame to see if compositor is being
  // destroyed midway which causes a crash. crbug.com/654672
  bool inside_main_frame_ = false;

  TaskGraphRunner* task_graph_runner_;

  SurfaceSequenceGenerator surface_sequence_generator_;
  uint32_t num_consecutive_frames_suitable_for_gpu_ = 0;

  scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
