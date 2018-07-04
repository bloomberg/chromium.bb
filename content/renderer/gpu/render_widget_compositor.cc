// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/task_scheduler/task_traits.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/region.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "content/renderer/gpu/render_widget_compositor_delegate.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_selection.h"
#include "ui/gfx/presentation_feedback.h"

namespace base {
class Value;
}

namespace cc {
class Layer;
}

namespace content {
namespace {

using ReportTimeCallback = blink::WebLayerTreeView::ReportTimeCallback;

class ReportTimeSwapPromise : public cc::SwapPromise {
 public:
  ReportTimeSwapPromise(ReportTimeCallback callback,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        base::WeakPtr<RenderWidgetCompositor> compositor);
  ~ReportTimeSwapPromise() override;

  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata* metadata) override;
  void DidSwap() override;
  void DidNotSwap(DidNotSwapReason reason) override;
  int64_t TraceId() const override;

 private:
  ReportTimeCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<RenderWidgetCompositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(ReportTimeSwapPromise);
};

ReportTimeSwapPromise::ReportTimeSwapPromise(
    ReportTimeCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<RenderWidgetCompositor> compositor)
    : callback_(std::move(callback)),
      task_runner_(std::move(task_runner)),
      compositor_(compositor) {}

ReportTimeSwapPromise::~ReportTimeSwapPromise() {}

void ReportTimeSwapPromise::WillSwap(viz::CompositorFrameMetadata* metadata) {
  DCHECK_GT(metadata->frame_token, 0u);
  metadata->request_presentation_feedback = true;
  auto* task_runner = task_runner_.get();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RenderWidgetCompositor::AddPresentationCallback, compositor_,
          metadata->frame_token,
          base::BindOnce(std::move(callback_),
                         blink::WebLayerTreeView::SwapResult::kDidSwap)));
}

void ReportTimeSwapPromise::DidSwap() {
  // If swap did happen, then the paint-time will be reported when the
  // presentation feedback is received.
}

void ReportTimeSwapPromise::DidNotSwap(
    cc::SwapPromise::DidNotSwapReason reason) {
  blink::WebLayerTreeView::SwapResult result;
  switch (reason) {
    case cc::SwapPromise::DidNotSwapReason::SWAP_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapSwapFails;
      break;
    case cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapCommitFails;
      break;
    case cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapCommitNoUpdate;
      break;
    case cc::SwapPromise::DidNotSwapReason::ACTIVATION_FAILS:
      result = blink::WebLayerTreeView::SwapResult::kDidNotSwapActivationFails;
      break;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback_), result,
                                                   base::TimeTicks::Now()));
}

int64_t ReportTimeSwapPromise::TraceId() const {
  return 0;
}

gfx::SelectionBound::Type ConvertFromWebSelectionBoundType(
    blink::WebSelectionBound::Type type) {
  if (type == blink::WebSelectionBound::Type::kSelectionLeft)
    return gfx::SelectionBound::Type::LEFT;
  if (type == blink::WebSelectionBound::Type::kSelectionRight)
    return gfx::SelectionBound::Type::RIGHT;
  // if WebSelection is not a range (caret or none),
  // The type of gfx::SelectionBound should be CENTER.
  DCHECK_EQ(type, blink::WebSelectionBound::Type::kCaret);
  return gfx::SelectionBound::Type::CENTER;
}

cc::LayerSelectionBound ConvertFromWebSelectionBound(
    const blink::WebSelectionBound& bound) {
  cc::LayerSelectionBound cc_bound;
  DCHECK(bound.layer_id);

  cc_bound.type = ConvertFromWebSelectionBoundType(bound.type);
  cc_bound.layer_id = bound.layer_id;
  cc_bound.edge_top = gfx::Point(bound.edge_top_in_layer);
  cc_bound.edge_bottom = gfx::Point(bound.edge_bottom_in_layer);
  cc_bound.hidden = bound.hidden;
  return cc_bound;
}

cc::LayerSelection ConvertFromWebSelection(
    const blink::WebSelection& web_selection) {
  if (web_selection.IsNone())
    return cc::LayerSelection();
  cc::LayerSelection cc_selection;
  cc_selection.start = ConvertFromWebSelectionBound(web_selection.Start());
  cc_selection.end = ConvertFromWebSelectionBound(web_selection.end());
  return cc_selection;
}

// Check cc::BrowserControlsState, and blink::WebBrowserControlsState
// are kept in sync.
static_assert(int(blink::kWebBrowserControlsBoth) == int(cc::BOTH),
              "mismatching enums: BOTH");
static_assert(int(blink::kWebBrowserControlsHidden) == int(cc::HIDDEN),
              "mismatching enums: HIDDEN");
static_assert(int(blink::kWebBrowserControlsShown) == int(cc::SHOWN),
              "mismatching enums: SHOWN");

static cc::BrowserControlsState ConvertBrowserControlsState(
    blink::WebBrowserControlsState state) {
  return static_cast<cc::BrowserControlsState>(state);
}

}  // namespace

RenderWidgetCompositor::RenderWidgetCompositor(
    RenderWidgetCompositorDelegate* delegate,
    CompositorDependencies* compositor_deps)
    : delegate_(delegate),
      compositor_deps_(compositor_deps),
      threaded_(!!compositor_deps_->GetCompositorImplThreadTaskRunner()),
      animation_host_(cc::AnimationHost::CreateMainInstance()),
      weak_factory_(this) {}

RenderWidgetCompositor::~RenderWidgetCompositor() = default;

void RenderWidgetCompositor::Initialize(const cc::LayerTreeSettings& settings) {
  const bool is_threaded =
      !!compositor_deps_->GetCompositorImplThreadTaskRunner();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.settings = &settings;
  params.task_graph_runner = compositor_deps_->GetTaskGraphRunner();
  params.main_task_runner =
      compositor_deps_->GetCompositorMainThreadTaskRunner();
  params.mutator_host = animation_host_.get();
  params.ukm_recorder_factory = compositor_deps_->CreateUkmRecorderFactory();
  if (base::TaskScheduler::GetInstance()) {
    // The image worker thread needs to allow waiting since it makes discardable
    // shared memory allocations which need to make synchronous calls to the
    // IO thread.
    params.image_worker_task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  }
  if (!is_threaded) {
    // Single-threaded layout tests.
    layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  } else {
    layer_tree_host_ = cc::LayerTreeHost::CreateThreaded(
        compositor_deps_->GetCompositorImplThreadTaskRunner(), &params);
  }
}

void RenderWidgetCompositor::SetNeverVisible() {
  DCHECK(!layer_tree_host_->IsVisible());
  never_visible_ = true;
}

const base::WeakPtr<cc::InputHandler>&
RenderWidgetCompositor::GetInputHandler() {
  return layer_tree_host_->GetInputHandler();
}

void RenderWidgetCompositor::SetNeedsDisplayOnAllLayers() {
  layer_tree_host_->SetNeedsDisplayOnAllLayers();
}

void RenderWidgetCompositor::SetRasterizeOnlyVisibleContent() {
  cc::LayerTreeDebugState current = layer_tree_host_->GetDebugState();
  current.rasterize_only_visible_content = true;
  layer_tree_host_->SetDebugState(current);
}

void RenderWidgetCompositor::SetNeedsRedrawRect(gfx::Rect damage_rect) {
  layer_tree_host_->SetNeedsRedrawRect(damage_rect);
}

bool RenderWidgetCompositor::IsSurfaceSynchronizationEnabled() const {
  return layer_tree_host_->GetSettings().enable_surface_synchronization;
}

void RenderWidgetCompositor::SetNeedsForcedRedraw() {
  layer_tree_host_->SetNeedsCommitWithForcedRedraw();
}

std::unique_ptr<cc::SwapPromiseMonitor>
RenderWidgetCompositor::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return std::make_unique<cc::LatencyInfoSwapPromiseMonitor>(
      latency, layer_tree_host_->GetSwapPromiseManager(), nullptr);
}

void RenderWidgetCompositor::QueueSwapPromise(
    std::unique_ptr<cc::SwapPromise> swap_promise) {
  layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
}

int RenderWidgetCompositor::GetSourceFrameNumber() const {
  return layer_tree_host_->SourceFrameNumber();
}

void RenderWidgetCompositor::NotifyInputThrottledUntilCommit() {
  layer_tree_host_->NotifyInputThrottledUntilCommit();
}

const cc::Layer* RenderWidgetCompositor::GetRootLayer() const {
  return layer_tree_host_->root_layer();
}

int RenderWidgetCompositor::ScheduleMicroBenchmark(
    const std::string& name,
    std::unique_ptr<base::Value> value,
    const base::Callback<void(std::unique_ptr<base::Value>)>& callback) {
  return layer_tree_host_->ScheduleMicroBenchmark(name, std::move(value),
                                                  callback);
}

bool RenderWidgetCompositor::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(value));
}

void RenderWidgetCompositor::SetViewportSizeAndScale(
    const gfx::Size& device_viewport_size,
    float device_scale_factor,
    const viz::LocalSurfaceId& local_surface_id) {
  layer_tree_host_->SetViewportSizeAndScale(
      device_viewport_size, device_scale_factor, local_surface_id);
}

void RenderWidgetCompositor::RequestNewLocalSurfaceId() {
  layer_tree_host_->RequestNewLocalSurfaceId();
}

bool RenderWidgetCompositor::HasNewLocalSurfaceIdRequest() const {
  return layer_tree_host_->new_local_surface_id_request_for_testing();
}

void RenderWidgetCompositor::SetViewportVisibleRect(
    const gfx::Rect& visible_rect) {
  layer_tree_host_->SetViewportVisibleRect(visible_rect);
}

viz::FrameSinkId RenderWidgetCompositor::GetFrameSinkId() {
  return frame_sink_id_;
}

void RenderWidgetCompositor::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  layer_tree_host_->SetRootLayer(std::move(layer));
}

void RenderWidgetCompositor::ClearRootLayer() {
  layer_tree_host_->SetRootLayer(nullptr);
}

cc::AnimationHost* RenderWidgetCompositor::CompositorAnimationHost() {
  return animation_host_.get();
}

blink::WebSize RenderWidgetCompositor::GetViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

blink::WebFloatPoint RenderWidgetCompositor::adjustEventPointForPinchZoom(
    const blink::WebFloatPoint& point) const {
  return point;
}

void RenderWidgetCompositor::SetBackgroundColor(SkColor color) {
  layer_tree_host_->set_background_color(color);
}

void RenderWidgetCompositor::SetVisible(bool visible) {
  if (never_visible_)
    return;

  layer_tree_host_->SetVisible(visible);

  if (visible && layer_tree_frame_sink_request_failed_while_invisible_)
    DidFailToInitializeLayerTreeFrameSink();
}

void RenderWidgetCompositor::SetPageScaleFactorAndLimits(
    float page_scale_factor,
    float minimum,
    float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(page_scale_factor, minimum,
                                                maximum);
}

void RenderWidgetCompositor::StartPageScaleAnimation(
    const blink::WebPoint& destination,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(
      gfx::Vector2d(destination.x, destination.y), use_anchor, new_page_scale,
      duration);
}

bool RenderWidgetCompositor::HasPendingPageScaleAnimation() const {
  return layer_tree_host_->HasPendingPageScaleAnimation();
}

void RenderWidgetCompositor::HeuristicsForGpuRasterizationUpdated(
    bool matches_heuristics) {
  layer_tree_host_->SetHasGpuRasterizationTrigger(matches_heuristics);
}

void RenderWidgetCompositor::SetNeedsBeginFrame() {
  layer_tree_host_->SetNeedsAnimate();
}

void RenderWidgetCompositor::DidStopFlinging() {
  layer_tree_host_->DidStopFlinging();
}

void RenderWidgetCompositor::RegisterViewportLayers(
    const blink::WebLayerTreeView::ViewportLayers& layers) {
  cc::LayerTreeHost::ViewportLayers viewport_layers;
  viewport_layers.overscroll_elasticity = layers.overscroll_elasticity;
  viewport_layers.page_scale = layers.page_scale;
  viewport_layers.inner_viewport_container = layers.inner_viewport_container;
  viewport_layers.outer_viewport_container = layers.outer_viewport_container;
  viewport_layers.inner_viewport_scroll = layers.inner_viewport_scroll;
  viewport_layers.outer_viewport_scroll = layers.outer_viewport_scroll;
  layer_tree_host_->RegisterViewportLayers(viewport_layers);
}

void RenderWidgetCompositor::ClearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(cc::LayerTreeHost::ViewportLayers());
}

void RenderWidgetCompositor::RegisterSelection(
    const blink::WebSelection& selection) {
  layer_tree_host_->RegisterSelection(ConvertFromWebSelection(selection));
}

void RenderWidgetCompositor::ClearSelection() {
  cc::LayerSelection empty_selection;
  layer_tree_host_->RegisterSelection(empty_selection);
}

void RenderWidgetCompositor::SetMutatorClient(
    std::unique_ptr<cc::LayerTreeMutator> client) {
  TRACE_EVENT0("cc", "RenderWidgetCompositor::setMutatorClient");
  layer_tree_host_->SetLayerTreeMutator(std::move(client));
}

void RenderWidgetCompositor::ForceRecalculateRasterScales() {
  layer_tree_host_->SetNeedsRecalculateRasterScales();
}

static_assert(static_cast<cc::EventListenerClass>(
                  blink::WebEventListenerClass::kTouchStartOrMove) ==
                  cc::EventListenerClass::kTouchStartOrMove,
              "EventListenerClass and WebEventListenerClass enums must match");
static_assert(static_cast<cc::EventListenerClass>(
                  blink::WebEventListenerClass::kMouseWheel) ==
                  cc::EventListenerClass::kMouseWheel,
              "EventListenerClass and WebEventListenerClass enums must match");

static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::kNothing) ==
                  cc::EventListenerProperties::kNone,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::kPassive) ==
                  cc::EventListenerProperties::kPassive,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::kBlocking) ==
                  cc::EventListenerProperties::kBlocking,
              "EventListener and WebEventListener enums must match");
static_assert(static_cast<cc::EventListenerProperties>(
                  blink::WebEventListenerProperties::kBlockingAndPassive) ==
                  cc::EventListenerProperties::kBlockingAndPassive,
              "EventListener and WebEventListener enums must match");

void RenderWidgetCompositor::SetEventListenerProperties(
    blink::WebEventListenerClass eventClass,
    blink::WebEventListenerProperties properties) {
  layer_tree_host_->SetEventListenerProperties(
      static_cast<cc::EventListenerClass>(eventClass),
      static_cast<cc::EventListenerProperties>(properties));
}

blink::WebEventListenerProperties
RenderWidgetCompositor::EventListenerProperties(
    blink::WebEventListenerClass event_class) const {
  return static_cast<blink::WebEventListenerProperties>(
      layer_tree_host_->event_listener_properties(
          static_cast<cc::EventListenerClass>(event_class)));
}

void RenderWidgetCompositor::SetHaveScrollEventHandlers(bool has_handlers) {
  layer_tree_host_->SetHaveScrollEventHandlers(has_handlers);
}

bool RenderWidgetCompositor::HaveScrollEventHandlers() const {
  return layer_tree_host_->have_scroll_event_handlers();
}

bool RenderWidgetCompositor::CompositeIsSynchronous() const {
  if (!threaded_) {
    DCHECK(!layer_tree_host_->GetSettings().single_thread_proxy_scheduler);
    return true;
  }
  return false;
}

void RenderWidgetCompositor::LayoutAndPaintAsync(base::OnceClosure callback) {
  DCHECK(layout_and_paint_async_callback_.is_null());
  layout_and_paint_async_callback_ = std::move(callback);

  if (CompositeIsSynchronous()) {
    // The LayoutAndPaintAsyncCallback is invoked in WillCommit, which is
    // dispatched after layout and paint for all compositing modes.
    const bool raster = false;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderWidgetCompositor::SynchronouslyComposite,
                       weak_factory_.GetWeakPtr(), raster, nullptr));
  } else {
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::SetLayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  if (!layer_tree_frame_sink) {
    DidFailToInitializeLayerTreeFrameSink();
    return;
  }
  layer_tree_host_->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void RenderWidgetCompositor::InvokeLayoutAndPaintCallback() {
  if (!layout_and_paint_async_callback_.is_null())
    std::move(layout_and_paint_async_callback_).Run();
}

void RenderWidgetCompositor::CompositeAndReadbackAsync(
    base::OnceCallback<void(const SkBitmap&)> callback) {
  DCHECK(layout_and_paint_async_callback_.is_null());
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner =
      layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner();
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              [](base::OnceCallback<void(const SkBitmap&)> callback,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 std::unique_ptr<viz::CopyOutputResult> result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), result->AsSkBitmap()));
              },
              std::move(callback), std::move(main_thread_task_runner)));
  auto swap_promise =
      delegate_->RequestCopyOfOutputForLayoutTest(std::move(request));

  // Force a commit to happen. The temporary copy output request will
  // be installed after layout which will happen as a part of the commit, for
  // widgets that delay the creation of their output surface.
  if (CompositeIsSynchronous()) {
    // Since the composite is required for a pixel dump, we need to raster.
    // Note that we defer queuing the SwapPromise until the requested Composite
    // with rasterization is done.
    const bool raster = true;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderWidgetCompositor::SynchronouslyComposite,
                       weak_factory_.GetWeakPtr(), raster,
                       std::move(swap_promise)));
  } else {
    // Force a redraw to ensure that the copy swap promise isn't cancelled due
    // to no damage.
    SetNeedsForcedRedraw();
    layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
    layer_tree_host_->SetNeedsCommit();
  }
}

void RenderWidgetCompositor::SynchronouslyCompositeNoRasterForTesting() {
  SynchronouslyComposite(false /* raster */, nullptr /* swap_promise */);
}

void RenderWidgetCompositor::CompositeWithRasterForTesting() {
  SynchronouslyComposite(true /* raster */, nullptr /* swap_promise */);
}

void RenderWidgetCompositor::SynchronouslyComposite(
    bool raster,
    std::unique_ptr<cc::SwapPromise> swap_promise) {
  DCHECK(CompositeIsSynchronous());
  if (!layer_tree_host_->IsVisible())
    return;

  if (in_synchronous_compositor_update_) {
    // LayoutTests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    delegate_->BeginMainFrame(base::TimeTicks::Now());
    delegate_->UpdateVisualState(
        cc::LayerTreeHostClient::VisualStateUpdate::kAll);
    return;
  }

  if (swap_promise) {
    // Force a redraw to ensure that the copy swap promise isn't cancelled due
    // to no damage.
    SetNeedsForcedRedraw();
    layer_tree_host_->QueueSwapPromise(std::move(swap_promise));
  }

  DCHECK(!in_synchronous_compositor_update_);
  base::AutoReset<bool> inside_composite(&in_synchronous_compositor_update_,
                                         true);
  layer_tree_host_->Composite(base::TimeTicks::Now(), raster);
}

void RenderWidgetCompositor::SetDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

int RenderWidgetCompositor::LayerTreeId() const {
  return layer_tree_host_->GetId();
}

void RenderWidgetCompositor::SetShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_fps_counter = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::SetShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_paint_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::SetShowDebugBorders(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  if (show)
    debug_state.show_debug_borders.set();
  else
    debug_state.show_debug_borders.reset();
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::SetShowScrollBottleneckRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->GetDebugState();
  debug_state.show_touch_event_handler_rects = show;
  debug_state.show_wheel_event_handler_rects = show;
  debug_state.show_non_fast_scrollable_rects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::UpdateBrowserControlsState(
    blink::WebBrowserControlsState constraints,
    blink::WebBrowserControlsState current,
    bool animate) {
  layer_tree_host_->UpdateBrowserControlsState(
      ConvertBrowserControlsState(constraints),
      ConvertBrowserControlsState(current), animate);
}

void RenderWidgetCompositor::SetBrowserControlsHeight(float top_height,
                                                      float bottom_height,
                                                      bool shrink) {
  layer_tree_host_->SetBrowserControlsHeight(top_height, bottom_height, shrink);
}

void RenderWidgetCompositor::SetBrowserControlsShownRatio(float ratio) {
  layer_tree_host_->SetBrowserControlsShownRatio(ratio);
}

void RenderWidgetCompositor::RequestDecode(
    const cc::PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  layer_tree_host_->QueueImageDecode(image, std::move(callback));

  // If we're compositing synchronously, the SetNeedsCommit call which will be
  // issued by |layer_tree_host_| is not going to cause a commit, due to the
  // fact that this would make layout tests slow and cause flakiness. However,
  // in this case we actually need a commit to transfer the decode requests to
  // the impl side. So, force a commit to happen.
  if (CompositeIsSynchronous()) {
    const bool raster = true;
    layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderWidgetCompositor::SynchronouslyComposite,
                       weak_factory_.GetWeakPtr(), raster, nullptr));
  }
}

void RenderWidgetCompositor::SetOverscrollBehavior(
    const cc::OverscrollBehavior& behavior) {
  layer_tree_host_->SetOverscrollBehavior(behavior);
}

void RenderWidgetCompositor::WillBeginMainFrame() {
  delegate_->WillBeginCompositorFrame();
}

void RenderWidgetCompositor::DidBeginMainFrame() {}

void RenderWidgetCompositor::BeginMainFrame(const viz::BeginFrameArgs& args) {
  compositor_deps_->GetWebMainThreadScheduler()->WillBeginFrame(args);
  delegate_->BeginMainFrame(args.frame_time);
}

void RenderWidgetCompositor::BeginMainFrameNotExpectedSoon() {
  compositor_deps_->GetWebMainThreadScheduler()->BeginFrameNotExpectedSoon();
}

void RenderWidgetCompositor::BeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {
  compositor_deps_->GetWebMainThreadScheduler()->BeginMainFrameNotExpectedUntil(
      time);
}

void RenderWidgetCompositor::UpdateLayerTreeHost(
    VisualStateUpdate requested_update) {
  delegate_->UpdateVisualState(requested_update);
}

void RenderWidgetCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  delegate_->ApplyViewportDeltas(inner_delta, outer_delta,
                                 elastic_overscroll_delta, page_scale,
                                 top_controls_delta);
}

void RenderWidgetCompositor::RecordWheelAndTouchScrollingCount(
    bool has_scrolled_by_wheel,
    bool has_scrolled_by_touch) {
  delegate_->RecordWheelAndTouchScrollingCount(has_scrolled_by_wheel,
                                               has_scrolled_by_touch);
}

void RenderWidgetCompositor::RequestNewLayerTreeFrameSink() {
  // If the host is closing, then no more compositing is possible.  This
  // prevents shutdown races between handling the close message and
  // the CreateLayerTreeFrameSink task.
  if (delegate_->IsClosing())
    return;
  delegate_->RequestNewLayerTreeFrameSink(
      base::Bind(&RenderWidgetCompositor::SetLayerTreeFrameSink,
                 weak_factory_.GetWeakPtr()));
}

void RenderWidgetCompositor::DidInitializeLayerTreeFrameSink() {
}

void RenderWidgetCompositor::DidFailToInitializeLayerTreeFrameSink() {
  if (!layer_tree_host_->IsVisible()) {
    layer_tree_frame_sink_request_failed_while_invisible_ = true;
    return;
  }
  layer_tree_frame_sink_request_failed_while_invisible_ = false;
  layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderWidgetCompositor::RequestNewLayerTreeFrameSink,
                     weak_factory_.GetWeakPtr()));
}

void RenderWidgetCompositor::WillCommit() {
  InvokeLayoutAndPaintCallback();
}

void RenderWidgetCompositor::DidCommit() {
  delegate_->DidCommitCompositorFrame();
  compositor_deps_->GetWebMainThreadScheduler()->DidCommitFrameToCompositor();
}

void RenderWidgetCompositor::DidCommitAndDrawFrame() {
  delegate_->DidCommitAndDrawCompositorFrame();
}

void RenderWidgetCompositor::DidReceiveCompositorFrameAck() {
  delegate_->DidReceiveCompositorFrameAck();
}

void RenderWidgetCompositor::DidCompletePageScaleAnimation() {
  delegate_->DidCompletePageScaleAnimation();
}

void RenderWidgetCompositor::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(layer_tree_host_->GetTaskRunnerProvider()
             ->MainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  while (!presentation_callbacks_.empty()) {
    const auto& front = presentation_callbacks_.begin();
    if (viz::FrameTokenGT(front->first, frame_token))
      break;
    for (auto& callback : front->second)
      std::move(callback).Run(feedback.timestamp);
    presentation_callbacks_.erase(front);
  }
}

void RenderWidgetCompositor::RequestScheduleAnimation() {
  delegate_->RequestScheduleAnimation();
}

void RenderWidgetCompositor::DidSubmitCompositorFrame() {}

void RenderWidgetCompositor::DidLoseLayerTreeFrameSink() {}

void RenderWidgetCompositor::SetFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  frame_sink_id_ = frame_sink_id;
}

void RenderWidgetCompositor::SetRasterColorSpace(
    const gfx::ColorSpace& color_space) {
  layer_tree_host_->SetRasterColorSpace(color_space);
}

void RenderWidgetCompositor::ClearCachesOnNextCommit() {
  layer_tree_host_->ClearCachesOnNextCommit();
}

void RenderWidgetCompositor::SetContentSourceId(uint32_t id) {
  layer_tree_host_->SetContentSourceId(id);
}

void RenderWidgetCompositor::NotifySwapTime(ReportTimeCallback callback) {
  QueueSwapPromise(std::make_unique<ReportTimeSwapPromise>(
      std::move(callback),
      layer_tree_host_->GetTaskRunnerProvider()->MainThreadTaskRunner(),
      weak_factory_.GetWeakPtr()));
}

void RenderWidgetCompositor::RequestBeginMainFrameNotExpected(bool new_state) {
  layer_tree_host_->RequestBeginMainFrameNotExpected(new_state);
}

void RenderWidgetCompositor::SetRenderFrameObserver(
    std::unique_ptr<cc::RenderFrameMetadataObserver> observer) {
  layer_tree_host_->SetRenderFrameObserver(std::move(observer));
}

void RenderWidgetCompositor::AddPresentationCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  if (!presentation_callbacks_.empty()) {
    auto& previous = presentation_callbacks_.back();
    uint32_t previous_frame_token = previous.first;
    if (previous_frame_token == frame_token) {
      previous.second.push_back(std::move(callback));
      DCHECK_LE(previous.second.size(), 250u);
      return;
    }
    DCHECK(viz::FrameTokenGT(frame_token, previous_frame_token));
  }
  std::vector<base::OnceCallback<void(base::TimeTicks)>> callbacks;
  callbacks.push_back(std::move(callback));
  presentation_callbacks_.push_back({frame_token, std::move(callbacks)});
  DCHECK_LE(presentation_callbacks_.size(), 25u);
}

void RenderWidgetCompositor::SetURLForUkm(const GURL& url) {
  layer_tree_host_->SetURLForUkm(url);
}

}  // namespace content
