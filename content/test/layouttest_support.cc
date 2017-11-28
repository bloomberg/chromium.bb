// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/layouttest_support.h"

#include <stddef.h>

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cc/test/pixel_test_output_surface.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/gpu_stream_constants.h"
#include "content/common/renderer.mojom.h"
#include "content/common/unique_name_helper.h"
#include "content/public/common/page_state.h"
#include "content/public/common/screen_info.h"
#include "content/public/renderer/renderer_gamepad_provider.h"
#include "content/renderer/fetchers/manifest_fetcher.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/layout_test_dependencies.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "device/sensors/public/cpp/motion_data.h"
#include "device/sensors/public/cpp/orientation_data.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/test/icc_profiles.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#elif defined(OS_WIN)
#include "content/child/font_warmup_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/win/direct_write.h"
#endif

using device::MotionData;
using device::OrientationData;
using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

base::LazyInstance<ViewProxyCreationCallback>::Leaky
    g_view_test_proxy_callback = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<WidgetProxyCreationCallback>::Leaky
    g_widget_test_proxy_callback = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<FrameProxyCreationCallback>::Leaky
    g_frame_test_proxy_callback = LAZY_INSTANCE_INITIALIZER;

using WebViewTestProxyType =
    test_runner::WebViewTestProxy<RenderViewImpl,
                                  CompositorDependencies*,
                                  const mojom::CreateViewParams&>;
using WebWidgetTestProxyType =
    test_runner::WebWidgetTestProxy<RenderWidget,
                                    int32_t,
                                    CompositorDependencies*,
                                    blink::WebPopupType,
                                    const ScreenInfo&,
                                    bool,
                                    bool,
                                    bool>;
using WebFrameTestProxyType =
    test_runner::WebFrameTestProxy<RenderFrameImpl,
                                   RenderFrameImpl::CreateParams>;

RenderViewImpl* CreateWebViewTestProxy(CompositorDependencies* compositor_deps,
                                       const mojom::CreateViewParams& params) {
  WebViewTestProxyType* render_view_proxy =
      new WebViewTestProxyType(compositor_deps, params);
  if (g_view_test_proxy_callback == nullptr)
    return render_view_proxy;
  g_view_test_proxy_callback.Get().Run(render_view_proxy, render_view_proxy);
  return render_view_proxy;
}

RenderWidget* CreateWebWidgetTestProxy(int32_t routing_id,
                                       CompositorDependencies* compositor_deps,
                                       blink::WebPopupType popup_type,
                                       const ScreenInfo& screen_info,
                                       bool swapped_out,
                                       bool hidden,
                                       bool never_visible) {
  WebWidgetTestProxyType* render_widget_proxy = new WebWidgetTestProxyType(
      routing_id, compositor_deps, popup_type, screen_info, swapped_out, hidden,
      never_visible);
  return render_widget_proxy;
}

void RenderWidgetInitialized(RenderWidget* render_widget) {
  WebWidgetTestProxyType* render_widget_proxy =
      static_cast<WebWidgetTestProxyType*>(render_widget);
  if (!g_widget_test_proxy_callback.Get().is_null()) {
    g_widget_test_proxy_callback.Get().Run(render_widget->GetWebWidget(),
                                           render_widget_proxy);
  }
}

RenderFrameImpl* CreateWebFrameTestProxy(RenderFrameImpl::CreateParams params) {
  WebFrameTestProxyType* render_frame_proxy =
      new WebFrameTestProxyType(std::move(params));
  if (g_frame_test_proxy_callback == nullptr)
    return render_frame_proxy;
  g_frame_test_proxy_callback.Get().Run(render_frame_proxy, render_frame_proxy);
  return render_frame_proxy;
}

float GetWindowToViewportScale(RenderWidget* render_widget) {
  blink::WebFloatRect rect(0, 0, 1.0f, 0.0);
  render_widget->ConvertWindowToViewport(&rect);
  return rect.width;
}

#if defined(OS_WIN)
// DirectWrite only has access to %WINDIR%\Fonts by default. For developer
// side-loading, support kRegisterFontFiles to allow access to additional fonts.
void RegisterSideloadedTypefaces(SkFontMgr* fontmgr) {
  for (const auto& file : switches::GetSideloadFontFiles()) {
    blink::WebFontRendering::AddSideloadedFontForTesting(
        fontmgr->makeFromFile(file.c_str()));
  }
}
#endif  // OS_WIN

}  // namespace

test_runner::WebViewTestProxyBase* GetWebViewTestProxyBase(
    RenderView* render_view) {
  WebViewTestProxyType* render_view_proxy =
      static_cast<WebViewTestProxyType*>(render_view);
  return static_cast<test_runner::WebViewTestProxyBase*>(render_view_proxy);
}

test_runner::WebFrameTestProxyBase* GetWebFrameTestProxyBase(
    RenderFrame* render_frame) {
  WebFrameTestProxyType* render_frame_proxy =
      static_cast<WebFrameTestProxyType*>(render_frame);
  return static_cast<test_runner::WebFrameTestProxyBase*>(render_frame_proxy);
}

test_runner::WebWidgetTestProxyBase* GetWebWidgetTestProxyBase(
    blink::WebLocalFrame* frame) {
  DCHECK(frame);
  RenderFrame* local_root = RenderFrame::FromWebFrame(frame->LocalRoot());
  DCHECK(local_root);
  // TODO(lfg): Simplify once RenderView no longer inherits from RenderWidget.
  if (local_root->IsMainFrame()) {
    test_runner::WebViewTestProxyBase* web_view_test_proxy_base =
        GetWebViewTestProxyBase(local_root->GetRenderView());
    auto* web_widget_test_proxy_base =
        static_cast<test_runner::WebWidgetTestProxyBase*>(
            web_view_test_proxy_base);
    DCHECK(web_widget_test_proxy_base->web_widget()->IsWebView());
    return web_widget_test_proxy_base;
  } else {
    RenderWidget* render_widget =
        static_cast<RenderFrameImpl*>(local_root)->GetRenderWidget();
    DCHECK(render_widget);
    WebWidgetTestProxyType* render_widget_proxy =
        static_cast<WebWidgetTestProxyType*>(render_widget);
    auto* web_widget_test_proxy_base =
        static_cast<test_runner::WebWidgetTestProxyBase*>(render_widget_proxy);
    DCHECK(web_widget_test_proxy_base->web_widget()->IsWebFrameWidget());
    return web_widget_test_proxy_base;
  }
}

RenderWidget* GetRenderWidget(
    test_runner::WebWidgetTestProxyBase* web_widget_test_proxy_base) {
  DCHECK(web_widget_test_proxy_base);

  blink::WebWidget* widget = web_widget_test_proxy_base->web_widget();
  // TODO(lfg): Simplify once RenderView no longer inherits from RenderWidget.
  if (widget->IsWebView()) {
    WebViewTestProxyType* render_view_proxy =
        static_cast<WebViewTestProxyType*>(web_widget_test_proxy_base);
    RenderViewImpl* render_view_impl =
        static_cast<RenderViewImpl*>(render_view_proxy);
    return render_view_impl;
  } else if (widget->IsWebFrameWidget()) {
    WebWidgetTestProxyType* render_widget_proxy =
        static_cast<WebWidgetTestProxyType*>(web_widget_test_proxy_base);
    return static_cast<RenderWidget*>(render_widget_proxy);
  } else {
    NOTREACHED();
    return nullptr;
  }
}

void EnableWebTestProxyCreation(
    const ViewProxyCreationCallback& view_proxy_creation_callback,
    const WidgetProxyCreationCallback& widget_proxy_creation_callback,
    const FrameProxyCreationCallback& frame_proxy_creation_callback) {
  g_view_test_proxy_callback.Get() = view_proxy_creation_callback;
  g_widget_test_proxy_callback.Get() = widget_proxy_creation_callback;
  g_frame_test_proxy_callback.Get() = frame_proxy_creation_callback;
  RenderViewImpl::InstallCreateHook(CreateWebViewTestProxy);
  RenderWidget::InstallCreateHook(CreateWebWidgetTestProxy,
                                  RenderWidgetInitialized);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void FetchManifestDoneCallback(std::unique_ptr<ManifestFetcher> fetcher,
                               const FetchManifestCallback& callback,
                               const blink::WebURLResponse& response,
                               const std::string& data) {
  // |fetcher| will be autodeleted here as it is going out of scope.
  callback.Run(response, data);
}

void FetchManifest(blink::WebView* view, const GURL& url,
                   const FetchManifestCallback& callback) {
  ManifestFetcher* fetcher = new ManifestFetcher(url);
  std::unique_ptr<ManifestFetcher> autodeleter(fetcher);

  // Start is called on fetcher which is also bound to the callback.
  // A raw pointer is used instead of a scoped_ptr as base::Passes passes
  // ownership and thus nulls the scoped_ptr. On MSVS this happens before
  // the call to Start, resulting in a crash.
  CHECK(view->MainFrame()->IsWebLocalFrame())
      << "This function cannot be called if the main frame is not a "
         "local frame.";
  fetcher->Start(view->MainFrame()->ToWebLocalFrame(), false,
                 base::Bind(&FetchManifestDoneCallback,
                            base::Passed(&autodeleter), callback));
}

void SetMockGamepadProvider(std::unique_ptr<RendererGamepadProvider> provider) {
  RenderThreadImpl::current_blink_platform_impl()
      ->SetPlatformEventObserverForTesting(blink::kWebPlatformEventTypeGamepad,
                                           std::move(provider));
}

void SetMockDeviceMotionData(const MotionData& data) {
  RendererBlinkPlatformImpl::SetMockDeviceMotionDataForTesting(data);
}

void SetMockDeviceOrientationData(const OrientationData& data) {
  RendererBlinkPlatformImpl::SetMockDeviceOrientationDataForTesting(data);
}

namespace {

// Invokes a callback on commit (on the main thread) to obtain the output
// surface that should be used, then asks that output surface to submit the copy
// request at SwapBuffers time.
class CopyRequestSwapPromise : public cc::SwapPromise {
 public:
  using FindLayerTreeFrameSinkCallback =
      base::Callback<viz::TestLayerTreeFrameSink*()>;
  CopyRequestSwapPromise(
      std::unique_ptr<viz::CopyOutputRequest> request,
      FindLayerTreeFrameSinkCallback find_layer_tree_frame_sink_callback)
      : copy_request_(std::move(request)),
        find_layer_tree_frame_sink_callback_(
            std::move(find_layer_tree_frame_sink_callback)) {}

  // cc::SwapPromise implementation.
  void OnCommit() override {
    layer_tree_frame_sink_from_commit_ =
        find_layer_tree_frame_sink_callback_.Run();
    DCHECK(layer_tree_frame_sink_from_commit_);
  }
  void DidActivate() override {}
  void WillSwap(viz::CompositorFrameMetadata*) override {
    layer_tree_frame_sink_from_commit_->RequestCopyOfOutput(
        std::move(copy_request_));
  }
  void DidSwap() override {}
  DidNotSwapAction DidNotSwap(DidNotSwapReason r) override {
    // The compositor should always swap in layout test mode.
    NOTREACHED() << "did not swap for reason " << r;
    return DidNotSwapAction::BREAK_PROMISE;
  }
  int64_t TraceId() const override { return 0; }

 private:
  std::unique_ptr<viz::CopyOutputRequest> copy_request_;
  FindLayerTreeFrameSinkCallback find_layer_tree_frame_sink_callback_;
  viz::TestLayerTreeFrameSink* layer_tree_frame_sink_from_commit_ = nullptr;
};

}  // namespace

class LayoutTestDependenciesImpl : public LayoutTestDependencies,
                                   public viz::TestLayerTreeFrameSinkClient {
 public:
  std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink(
      int32_t routing_id,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::ContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      CompositorDependencies* deps) override {
    // This could override the GpuChannel for a LayerTreeFrameSink that was
    // previously being created but in that case the old GpuChannel would be
    // lost as would the LayerTreeFrameSink.
    gpu_channel_ = gpu_channel;

    auto* task_runner = deps->GetCompositorImplThreadTaskRunner().get();
    bool synchronous_composite = !task_runner;
    if (!task_runner)
      task_runner = base::ThreadTaskRunnerHandle::Get().get();

    viz::RendererSettings renderer_settings;
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    renderer_settings.allow_antialiasing &=
        !cmd->HasSwitch(cc::switches::kDisableCompositedAntialiasing);
    renderer_settings.highp_threshold_min = 2048;

    constexpr bool disable_display_vsync = false;
    constexpr double refresh_rate = 60.0;
    auto layer_tree_frame_sink = std::make_unique<viz::TestLayerTreeFrameSink>(
        std::move(compositor_context_provider),
        std::move(worker_context_provider), nullptr /* shared_bitmap_manager */,
        gpu_memory_buffer_manager, renderer_settings, task_runner,
        synchronous_composite, disable_display_vsync, refresh_rate);
    layer_tree_frame_sink->SetClient(this);
    layer_tree_frame_sinks_[routing_id] = layer_tree_frame_sink.get();
    return std::move(layer_tree_frame_sink);
  }

  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutput(
      int32_t routing_id,
      std::unique_ptr<viz::CopyOutputRequest> request) override {
    // Note that we can't immediately check layer_tree_frame_sinks_, since it
    // may not have been created yet. Instead, we wait until OnCommit to find
    // the currently active LayerTreeFrameSink for the given RenderWidget
    // routing_id.
    return std::make_unique<CopyRequestSwapPromise>(
        std::move(request),
        base::Bind(
            &LayoutTestDependenciesImpl::FindLayerTreeFrameSink,
            // |this| will still be valid, because its lifetime is tied to
            // RenderThreadImpl, which outlives layout test execution.
            base::Unretained(this), routing_id));
  }

  // TestLayerTreeFrameSinkClient implementation.
  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    // This is for an offscreen context for the compositor. So the default
    // framebuffer doesn't need alpha, depth, stencil, antialiasing.
    gpu::gles2::ContextCreationAttribHelper attributes;
    attributes.alpha_size = -1;
    attributes.depth_size = 0;
    attributes.stencil_size = 0;
    attributes.samples = 0;
    attributes.sample_buffers = 0;
    attributes.bind_generates_resource = false;
    attributes.lose_context_when_out_of_memory = true;
    const bool automatic_flushes = false;
    const bool support_locking = false;

    auto context_provider =
        base::MakeRefCounted<ui::ContextProviderCommandBuffer>(
            gpu_channel_, kGpuStreamIdDefault, kGpuStreamPriorityDefault,
            gpu::kNullSurfaceHandle,
            GURL("chrome://gpu/"
                 "LayoutTestDependenciesImpl::CreateOutputSurface"),
            automatic_flushes, support_locking, gpu::SharedMemoryLimits(),
            attributes, nullptr,
            ui::command_buffer_metrics::OFFSCREEN_CONTEXT_FOR_TESTING);
    context_provider->BindToCurrentThread();

    bool flipped_output_surface = false;
    return std::make_unique<cc::PixelTestOutputSurface>(
        std::move(context_provider), flipped_output_surface);
  }
  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const viz::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  viz::TestLayerTreeFrameSink* FindLayerTreeFrameSink(int32_t routing_id) {
    auto it = layer_tree_frame_sinks_.find(routing_id);
    return it == layer_tree_frame_sinks_.end() ? nullptr : it->second;
  }

  // Entries are not removed, so this map can grow. However, it is only used in
  // layout tests, so this memory usage does not occur in production.
  // Entries in this map will outlive the output surface, because this object is
  // owned by RenderThreadImpl, which outlives layout test execution.
  std::unordered_map<int32_t, viz::TestLayerTreeFrameSink*>
      layer_tree_frame_sinks_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
};

void EnableRendererLayoutTestMode() {
  RenderThreadImpl::current()->set_layout_test_dependencies(
      std::make_unique<LayoutTestDependenciesImpl>());

  UniqueNameHelper::PreserveStableUniqueNameForTesting();

#if defined(OS_WIN)
  RegisterSideloadedTypefaces(SkFontMgr_New_DirectWrite().get());
#endif
}

void EnableBrowserLayoutTestMode() {
#if defined(OS_MACOSX)
  gpu::ImageTransportSurface::SetAllowOSMesaForTesting(true);
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void TerminateAllSharedWorkersForTesting(base::OnceClosure callback) {
  static_cast<SharedWorkerServiceImpl*>(SharedWorkerService::GetInstance())
      ->TerminateAllWorkersForTesting(std::move(callback));
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)->
      GetLocalSessionHistoryLengthForTesting();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view,
                           const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  render_view_impl->ForceResizeForTesting(new_size);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  static_cast<RenderViewImpl*>(render_view)->
      SetDeviceScaleFactorForTesting(factor);
}

float GetWindowToViewportScale(RenderView* render_view) {
  return GetWindowToViewportScale(
      static_cast<RenderViewImpl*>(render_view)->GetWidget());
}

std::unique_ptr<blink::WebInputEvent> TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxyBase* web_widget_test_proxy_base,
    const blink::WebInputEvent& event) {
  DCHECK(web_widget_test_proxy_base);
  RenderWidget* render_widget = GetRenderWidget(web_widget_test_proxy_base);

  blink::WebRect view_rect = render_widget->ViewRect();
  float scale = GetWindowToViewportScale(render_widget);
  gfx::Vector2d delta(-view_rect.x, -view_rect.y);
  return ui::TranslateAndScaleWebInputEvent(event, delta, scale);
}

gfx::ColorSpace GetTestingColorSpace(const std::string& name) {
  if (name == "genericRGB") {
    return gfx::ICCProfileForTestingGenericRGB().GetColorSpace();
  } else if (name == "sRGB") {
    return gfx::ColorSpace::CreateSRGB();
  } else if (name == "test" || name == "colorSpin") {
    return gfx::ICCProfileForTestingColorSpin().GetColorSpace();
  } else if (name == "adobeRGB") {
    return gfx::ICCProfileForTestingAdobeRGB().GetColorSpace();
  } else if (name == "reset") {
    return display::Display::GetForcedColorProfile();
  }
  return gfx::ColorSpace();
}

void SetDeviceColorSpace(RenderView* render_view,
                         const gfx::ColorSpace& color_space) {
  static_cast<RenderViewImpl*>(render_view)
      ->SetDeviceColorSpaceForTesting(color_space);
}

void SetTestBluetoothScanDuration() {
  BluetoothDeviceChooserController::SetTestScanDurationForTesting();
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  static_cast<RenderViewImpl*>(render_view)->
      EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  static_cast<RenderViewImpl*>(render_view)->
      DisableAutoResizeForTesting(new_size);
}

void SchedulerRunIdleTasks(const base::Closure& callback) {
  blink::scheduler::RendererScheduler* scheduler =
      content::RenderThreadImpl::current()->GetRendererScheduler();
  blink::scheduler::RunIdleTasksForTesting(scheduler, callback);
}

void ForceTextInputStateUpdateForRenderFrame(RenderFrame* frame) {
  if (auto* render_widget =
          static_cast<RenderFrameImpl*>(frame)->GetRenderWidget()) {
    render_widget->ShowVirtualKeyboard();
  }
}

bool IsNavigationInitiatedByRenderer(const blink::WebURLRequest& request) {
  RequestExtraData* extra_data =
      static_cast<RequestExtraData*>(request.GetExtraData());
  return extra_data && extra_data->navigation_initiated_by_renderer();
}

}  // namespace content
