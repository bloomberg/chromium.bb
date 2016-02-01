// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/base/switches.h"
#include "cc/blink/web_external_bitmap_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer_settings.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/scheduler/child/compositor_worker_scheduler.h"
#include "components/scheduler/child/webthread_base.h"
#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "content/child/appcache/appcache_dispatcher.h"
#include "content/child/appcache/appcache_frontend_impl.h"
#include "content/child/child_discardable_shared_memory_manager.h"
#include "content/child/child_gpu_memory_buffer_manager.h"
#include "content/child/child_histogram_message_filter.h"
#include "content/child/child_resource_message_filter.h"
#include "content/child/child_shared_bitmap_manager.h"
#include "content/child/content_child_helpers.h"
#include "content/child/db_message_filter.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_message_filter.h"
#include "content/child/npapi/npobject_util.h"
#include "content/child/plugin_messages.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/resource_scheduling_filter.h"
#include "content/child/runtime_features.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_database_observer_impl.h"
#include "content/child/worker_thread_registry.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/database_messages.h"
#include "content/common/dom_storage/dom_storage_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/render_frame_setup.mojom.h"
#include "content/common/render_process_messages.h"
#include "content/common/resource_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_channel_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/bluetooth/bluetooth_message_filter.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/cache_storage/cache_storage_dispatcher.h"
#include "content/renderer/cache_storage/cache_storage_message_filter.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/devtools/v8_sampling_profiler.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gpu/compositor_external_begin_frame_source.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/input/input_event_filter.h"
#include "content/renderer/input/input_handler_manager.h"
#include "content/renderer/input/main_thread_input_event_filter.h"
#include "content/renderer/media/aec_dump_message_filter.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/media_stream_center.h"
#include "content/renderer/media/midi_message_filter.h"
#include "content/renderer/media/render_media_client.h"
#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "content/renderer/net_info_helper.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/raster_worker_pool.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/scheduler/resource_dispatch_throttler.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "content/renderer/service_worker/service_worker_context_message_filter.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "gin/public/debug.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "third_party/WebKit/public/platform/WebImageGenerator.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebMemoryPressureListener.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/android/synchronous_compositor_external_begin_frame_source.h"
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/android/synchronous_compositor_filter.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#include "content/renderer/media/android/stream_texture_factory_impl.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "content/renderer/theme_helper_mac.h"
#include "content/renderer/webscrollbarbehavior_impl_mac.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#else
// TODO(port)
#include "content/child/npapi/np_channel_base.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/renderer/npapi/plugin_channel_host.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_identity_service.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

#if defined(MOJO_SHELL_CLIENT)
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/mus/render_widget_window_tree_client_factory.h"
#endif

using base::ThreadRestrictions;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebNetworkStateNotifier;
using blink::WebRuntimeFeatures;
using blink::WebScriptController;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;
using scheduler::WebThreadImplForWorkerScheduler;

namespace content {

namespace {

const int64_t kInitialIdleHandlerDelayMs = 1000;
const int64_t kLongIdleHandlerDelayMs = 30 * 1000;

#if defined(OS_ANDROID)
// On Android, resource messages can each take ~1.5ms to dispatch on the browser
// IO thread. Limiting the message rate to 3/frame at 60hz ensures that the
// induced work takes but a fraction (~1/4) of the overall frame budget.
const int kMaxResourceRequestsPerFlushWhenThrottled = 3;
#else
const int kMaxResourceRequestsPerFlushWhenThrottled = 8;
#endif
const double kThrottledResourceRequestFlushPeriodS = 1. / 60.;

// Maximum allocation size allowed for image scaling filters that
// require pre-scaling. Skia will fallback to a filter that doesn't
// require pre-scaling if the default filter would require an
// allocation that exceeds this limit.
const size_t kImageCacheSingleAllocationByteLimit = 64 * 1024 * 1024;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

class WebThreadForCompositor : public WebThreadImplForWorkerScheduler {
 public:
  explicit WebThreadForCompositor(base::Thread::Options options)
      : WebThreadImplForWorkerScheduler("Compositor", options) {
    Init();
  }
  ~WebThreadForCompositor() override {}

 private:
  // WebThreadImplForWorkerScheduler:
  scoped_ptr<scheduler::WorkerScheduler> CreateWorkerScheduler() override {
    return make_scoped_ptr(new scheduler::CompositorWorkerScheduler(thread()));
  }

  DISALLOW_COPY_AND_ASSIGN(WebThreadForCompositor);
};

class RenderViewZoomer : public RenderViewVisitor {
 public:
  RenderViewZoomer(const std::string& scheme,
                   const std::string& host,
                   double zoom_level) : scheme_(scheme),
                                        host_(host),
                                        zoom_level_(zoom_level) {
  }

  bool Visit(RenderView* render_view) override {
    WebView* webview = render_view->GetWebView();
    RenderViewImpl* render_view_impl =
        static_cast<RenderViewImpl*>(render_view);
    // Remote frames don't host documents.
    // TODO(wjmaclean) Although it seems likely that a frame without a
    // document can safely early-out here, we should confirm this is truly
    // the case. https://crbug.com/477007
    if (webview->mainFrame()->isWebRemoteFrame())
      return true;

    WebDocument document = webview->mainFrame()->document();

    // Don't set zoom level for full-page plugin since they don't use the same
    // zoom settings.
    if (document.isPluginDocument())
      return true;
    GURL url(document.url());
    // Empty scheme works as wildcard that matches any scheme,
    if ((net::GetHostOrSpecFromURL(url) == host_) &&
        (scheme_.empty() || scheme_ == url.scheme()) &&
        !render_view_impl->uses_temporary_zoom_level()) {
      webview->hidePopups();
      render_view_impl->SetZoomLevel(zoom_level_);
    }
    return true;
  }

 private:
  const std::string scheme_;
  const std::string host_;
  const double zoom_level_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewZoomer);
};

void* CreateHistogram(
    const char *name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  std::string histogram_name;
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  if (render_thread_impl) {  // Can be null in tests.
    histogram_name = render_thread_impl->
        histogram_customizer()->ConvertToCustomHistogramName(name);
  } else {
    histogram_name = std::string(name);
  }
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      histogram_name, min, max, buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

scoped_ptr<cc::SharedBitmap> AllocateSharedBitmapFunction(
    const gfx::Size& size) {
  return ChildThreadImpl::current()->shared_bitmap_manager()->
      AllocateSharedBitmap(size);
}

void EnableBlinkPlatformLogChannels(const std::string& channels) {
  if (channels.empty())
    return;
  base::StringTokenizer t(channels, ", ");
  while (t.GetNext())
    blink::enableLogChannel(t.token().c_str());
}

void NotifyTimezoneChangeOnThisThread() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate)
    return;
  v8::Date::DateTimeConfigurationChangeNotification(isolate);
}

void LowMemoryNotificationOnThisThread() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate)
    return;
  isolate->LowMemoryNotification();
}

class RenderFrameSetupImpl : public RenderFrameSetup {
 public:
  explicit RenderFrameSetupImpl(
      mojo::InterfaceRequest<RenderFrameSetup> request)
      : routing_id_highmark_(-1), binding_(this, std::move(request)) {}

  void ExchangeServiceProviders(
      int32_t frame_routing_id,
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services)
      override {
    // TODO(morrita): This is for investigating http://crbug.com/415059 and
    // should be removed once it is fixed.
    CHECK_LT(routing_id_highmark_, frame_routing_id);
    routing_id_highmark_ = frame_routing_id;

    RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(frame_routing_id);
    // We can receive a GetServiceProviderForFrame message for a frame not yet
    // created due to a race between the message and a ViewMsg_New IPC that
    // triggers creation of the RenderFrame we want.
    if (!frame) {
      RenderThreadImpl::current()->RegisterPendingRenderFrameConnect(
          frame_routing_id, std::move(services), std::move(exposed_services));
      return;
    }

    frame->BindServiceRegistry(std::move(services),
                               std::move(exposed_services));
  }

 private:
  int32_t routing_id_highmark_;
  mojo::StrongBinding<RenderFrameSetup> binding_;
};

void CreateRenderFrameSetup(mojo::InterfaceRequest<RenderFrameSetup> request) {
  new RenderFrameSetupImpl(std::move(request));
}

blink::WebGraphicsContext3D::Attributes GetOffscreenAttribs() {
  blink::WebGraphicsContext3D::Attributes attributes;
  attributes.shareResources = true;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.antialias = false;
  attributes.noAutomaticFlushes = true;
  return attributes;
}

void SetupEmbeddedWorkerOnWorkerThread(
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::InterfacePtrInfo<mojo::ServiceProvider> exposed_services) {
  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  // It is possible for client to be null if for some reason the worker died
  // before this call made it to the worker thread. In that case just do
  // nothing and let mojo close the connection.
  if (!client)
    return;
  client->BindServiceRegistry(std::move(services),
                              mojo::MakeProxy(std::move(exposed_services)));
}

class EmbeddedWorkerSetupImpl : public EmbeddedWorkerSetup {
 public:
  explicit EmbeddedWorkerSetupImpl(
      mojo::InterfaceRequest<EmbeddedWorkerSetup> request)
      : binding_(this, std::move(request)) {}

  void ExchangeServiceProviders(
      int32_t thread_id,
      mojo::InterfaceRequest<mojo::ServiceProvider> services,
      mojo::ServiceProviderPtr exposed_services) override {
    WorkerThreadRegistry::Instance()->GetTaskRunnerFor(thread_id)->PostTask(
        FROM_HERE,
        base::Bind(&SetupEmbeddedWorkerOnWorkerThread, base::Passed(&services),
                   base::Passed(exposed_services.PassInterface())));
  }

 private:
  mojo::StrongBinding<EmbeddedWorkerSetup> binding_;
};

void CreateEmbeddedWorkerSetup(
    mojo::InterfaceRequest<EmbeddedWorkerSetup> request) {
  new EmbeddedWorkerSetupImpl(std::move(request));
}

void StringToUintVector(const std::string& str, std::vector<unsigned>* vector) {
  DCHECK(vector->empty());
  std::vector<std::string> pieces = base::SplitString(
      str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(pieces.size(), static_cast<size_t>(gfx::BufferFormat::LAST) + 1);
  for (size_t i = 0; i < pieces.size(); ++i) {
    unsigned number = 0;
    bool succeed = base::StringToUint(pieces[i], &number);
    DCHECK(succeed);
    vector->push_back(number);
  }
}

}  // namespace

// For measuring memory usage after each task. Behind a command line flag.
class MemoryObserver : public base::MessageLoop::TaskObserver {
 public:
  MemoryObserver() {}
  ~MemoryObserver() override {}

  void WillProcessTask(const base::PendingTask& pending_task) override {}

  void DidProcessTask(const base::PendingTask& pending_task) override {
    LOCAL_HISTOGRAM_MEMORY_KB("Memory.RendererUsed", GetMemoryUsageKB());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryObserver);
};

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
  custom_histograms_.insert("V8.MemoryHeapUsed");
  custom_histograms_.insert("V8.MemoryHeapCommitted");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host, size_t view_count) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHistogramCustomizer)) {
    return;
  }
  // Check if all RenderViews are displaying a page from the same host. If there
  // is only one RenderView, the common host is this view's host. If there are
  // many, check if this one shares the common host of the other
  // RenderViews. It's ok to not detect some cases where the RenderViews share a
  // common host. This information is only used for producing custom histograms.
  if (view_count == 1)
    SetCommonHost(host);
  else if (host != common_host_)
    SetCommonHost(std::string());
}

std::string RenderThreadImpl::HistogramCustomizer::ConvertToCustomHistogramName(
    const char* histogram_name) const {
  std::string name(histogram_name);
  if (!common_host_histogram_suffix_.empty() &&
      custom_histograms_.find(name) != custom_histograms_.end())
    name += common_host_histogram_suffix_;
  return name;
}

void RenderThreadImpl::HistogramCustomizer::SetCommonHost(
    const std::string& host) {
  if (host != common_host_) {
    common_host_ = host;
    common_host_histogram_suffix_ = HostToCustomHistogramSuffix(host);
    blink::mainThreadIsolate()->SetCreateHistogramFunction(CreateHistogram);
  }
}

std::string RenderThreadImpl::HistogramCustomizer::HostToCustomHistogramSuffix(
    const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  if (host == "inbox.google.com")
    return ".inbox";
  if (host == "www.youtube.com")
    return ".youtube";
  if (IsAlexaTop10NonGoogleSite(host))
    return ".top10";

  return std::string();
}

bool RenderThreadImpl::HistogramCustomizer::IsAlexaTop10NonGoogleSite(
    const std::string& host) {
  // The Top10 sites have different TLD and/or subdomains depending on the
  // localization.
  if (host == "sina.com.cn")
    return true;

  std::string sanitized_host =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (sanitized_host == "facebook.com")
    return true;
  if (sanitized_host == "baidu.com")
    return true;
  if (sanitized_host == "qq.com")
    return true;
  if (sanitized_host == "twitter.com")
    return true;
  if (sanitized_host == "taobao.com")
    return true;
  if (sanitized_host == "live.com")
    return true;

  if (!sanitized_host.empty()) {
    std::vector<base::StringPiece> host_tokens = base::SplitStringPiece(
        sanitized_host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (host_tokens.size() >= 2) {
      if ((host_tokens[0] == "yahoo") || (host_tokens[0] == "amazon") ||
          (host_tokens[0] == "wikipedia")) {
        return true;
      }
    }
  }
  return false;
}

// static
RenderThreadImpl* RenderThreadImpl::Create(
    const InProcessChildThreadParams& params) {
  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler =
      scheduler::RendererScheduler::Create();
  return new RenderThreadImpl(params, std::move(renderer_scheduler));
}

// static
RenderThreadImpl* RenderThreadImpl::Create(
    scoped_ptr<base::MessageLoop> main_message_loop,
    scoped_ptr<scheduler::RendererScheduler> renderer_scheduler) {
  return new RenderThreadImpl(std::move(main_message_loop),
                              std::move(renderer_scheduler));
}

RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

RenderThreadImpl::RenderThreadImpl(
    const InProcessChildThreadParams& params,
    scoped_ptr<scheduler::RendererScheduler> scheduler)
    : ChildThreadImpl(Options::Builder()
                          .InBrowserProcess(params)
                          .UseMojoChannel(ShouldUseMojoChannel())
                          .Build()),
      renderer_scheduler_(std::move(scheduler)),
      raster_worker_pool_(new RasterWorkerPool()) {
  Init();
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThreadImpl::RenderThreadImpl(
    scoped_ptr<base::MessageLoop> main_message_loop,
    scoped_ptr<scheduler::RendererScheduler> scheduler)
    : ChildThreadImpl(Options::Builder()
                          .UseMojoChannel(ShouldUseMojoChannel())
                          .Build()),
      renderer_scheduler_(std::move(scheduler)),
      main_message_loop_(std::move(main_message_loop)),
      raster_worker_pool_(new RasterWorkerPool()) {
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT0("startup", "RenderThreadImpl::Init");

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if defined(OS_MACOSX) || (defined(OS_ANDROID) && !defined(USE_AURA))
  // On Mac and Android Java UI, the select popups are rendered by the browser.
  blink::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  // In single process the single process is all there is.
  notify_webkit_of_modal_loop_ = true;
  webkit_shared_timer_suspended_ = false;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_ms_ = kInitialIdleHandlerDelayMs;
  idle_notifications_to_skip_ = 0;
  layout_test_mode_ = false;

  appcache_dispatcher_.reset(
      new AppCacheDispatcher(Get(), new AppCacheFrontendImpl()));
  dom_storage_dispatcher_.reset(new DomStorageDispatcher());
  main_thread_indexed_db_dispatcher_.reset(new IndexedDBDispatcher(
      thread_safe_sender()));
  main_thread_cache_storage_dispatcher_.reset(
      new CacheStorageDispatcher(thread_safe_sender()));
  embedded_worker_dispatcher_.reset(new EmbeddedWorkerDispatcher());

  // Note: This may reorder messages from the ResourceDispatcher with respect to
  // other subsystems.
  resource_dispatch_throttler_.reset(new ResourceDispatchThrottler(
      static_cast<RenderThread*>(this), renderer_scheduler_.get(),
      base::TimeDelta::FromSecondsD(kThrottledResourceRequestFlushPeriodS),
      kMaxResourceRequestsPerFlushWhenThrottled));
  resource_dispatcher()->set_message_sender(resource_dispatch_throttler_.get());

  media_stream_center_ = NULL;

  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

  vc_manager_.reset(new VideoCaptureImplManager());
  AddFilter(vc_manager_->video_capture_message_filter());

  browser_plugin_manager_.reset(new BrowserPluginManager());
  AddObserver(browser_plugin_manager_.get());

#if defined(ENABLE_WEBRTC)
  peer_connection_tracker_.reset(new PeerConnectionTracker());
  AddObserver(peer_connection_tracker_.get());

  p2p_socket_dispatcher_ =
      new P2PSocketDispatcher(GetIOMessageLoopProxy().get());
  AddFilter(p2p_socket_dispatcher_.get());

  webrtc_identity_service_.reset(new WebRTCIdentityService());

  peer_connection_factory_.reset(
      new PeerConnectionDependencyFactory(p2p_socket_dispatcher_.get()));

  aec_dump_message_filter_ = new AecDumpMessageFilter(
      GetIOMessageLoopProxy(), message_loop()->task_runner(),
      peer_connection_factory_.get());

  AddFilter(aec_dump_message_filter_.get());

#endif  // defined(ENABLE_WEBRTC)

  audio_input_message_filter_ =
      new AudioInputMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_message_filter_.get());

  midi_message_filter_ = new MidiMessageFilter(GetIOMessageLoopProxy());
  AddFilter(midi_message_filter_.get());

  bluetooth_message_filter_ = new BluetoothMessageFilter(thread_safe_sender());
  AddFilter(bluetooth_message_filter_->GetFilter());

  AddFilter((new IndexedDBMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new CacheStorageMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new ServiceWorkerContextMessageFilter())->GetFilter());

  GetContentClient()->renderer()->RenderThreadStarted();

  InitSkiaEventTracer();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  cc::LayerSettings layer_settings;
  layer_settings.use_compositor_animation_timelines =
      !command_line.HasSwitch(switches::kDisableCompositorAnimationTimelines);
  cc_blink::WebLayerImpl::SetLayerSettings(layer_settings);
  cc::SetClientNameForMetrics("Renderer");

  is_threaded_animation_enabled_ =
      !command_line.HasSwitch(cc::switches::kDisableThreadedAnimation);

  is_zero_copy_enabled_ = command_line.HasSwitch(switches::kEnableZeroCopy);
  is_partial_raster_enabled_ =
      command_line.HasSwitch(switches::kEnablePartialRaster);
  is_gpu_memory_buffer_compositor_resources_enabled_ = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  is_elastic_overscroll_enabled_ = base::mac::IsOSLionOrLater();
  if (is_elastic_overscroll_enabled_) {
    base::ScopedCFTypeRef<CFStringRef> key(
        base::SysUTF8ToCFStringRef("NSScrollViewRubberbanding"));
    Boolean key_exists = false;
    Boolean value = CFPreferencesGetAppBooleanValue(
        key, kCFPreferencesCurrentApplication, &key_exists);
    if (key_exists && !value)
      is_elastic_overscroll_enabled_ = false;
  }
#else
  is_elastic_overscroll_enabled_ = false;
#endif

  std::string image_texture_target_string =
      command_line.GetSwitchValueASCII(switches::kContentImageTextureTarget);
  StringToUintVector(image_texture_target_string, &use_image_texture_targets_);

  if (command_line.HasSwitch(switches::kDisableLCDText)) {
    is_lcd_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableLCDText)) {
    is_lcd_text_enabled_ = true;
  } else {
#if defined(OS_ANDROID)
    is_lcd_text_enabled_ = false;
#else
    is_lcd_text_enabled_ = true;
#endif
  }

  is_gpu_rasterization_enabled_ =
      command_line.HasSwitch(switches::kEnableGpuRasterization);
  is_gpu_rasterization_forced_ =
      command_line.HasSwitch(switches::kForceGpuRasterization);

  if (command_line.HasSwitch(switches::kGpuRasterizationMSAASampleCount)) {
    std::string string_value = command_line.GetSwitchValueASCII(
        switches::kGpuRasterizationMSAASampleCount);
    bool parsed_msaa_sample_count =
        base::StringToInt(string_value, &gpu_rasterization_msaa_sample_count_);
    DCHECK(parsed_msaa_sample_count) << string_value;
    DCHECK_GE(gpu_rasterization_msaa_sample_count_, 0);
  } else {
    gpu_rasterization_msaa_sample_count_ = -1;
  }

  if (command_line.HasSwitch(switches::kDisableDistanceFieldText)) {
    is_distance_field_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableDistanceFieldText)) {
    is_distance_field_text_enabled_ = true;
  } else {
    is_distance_field_text_enabled_ = false;
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  media::InitializeMediaLibrary();

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&RenderThreadImpl::OnMemoryPressure, base::Unretained(this))));

  int num_raster_threads = 0;
  std::string string_value =
      command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
  bool parsed_num_raster_threads =
      base::StringToInt(string_value, &num_raster_threads);
  DCHECK(parsed_num_raster_threads) << string_value;
  DCHECK_GT(num_raster_threads, 0);

#if defined(OS_ANDROID)
  // Note: Currently, enabling image decode tasks only provides a benefit if
  // we use high quality interpolation filters, which are disabled on android.
  are_image_decode_tasks_enabled_ = false;
#else
  are_image_decode_tasks_enabled_ = true;
#endif

  base::SimpleThread::Options thread_options;
#if defined(OS_ANDROID) || defined(OS_LINUX)
  if (!command_line.HasSwitch(
          switches::kUseNormalPriorityForTileTaskWorkerThreads)) {
    thread_options.set_priority(base::ThreadPriority::BACKGROUND);
  }
#endif

  raster_worker_pool_->Start(num_raster_threads, thread_options);

  // TODO(boliu): In single process, browser main loop should set up the
  // discardable memory manager, and should skip this if kSingleProcess.
  // See crbug.com/503724.
  base::DiscardableMemoryAllocator::SetInstance(
      ChildThreadImpl::discardable_shared_memory_manager());

  service_registry()->AddService<RenderFrameSetup>(
      base::Bind(CreateRenderFrameSetup));
  service_registry()->AddService<EmbeddedWorkerSetup>(
      base::Bind(CreateEmbeddedWorkerSetup));

#if defined(MOJO_SHELL_CLIENT)
  // We may not have a MojoShellConnection object in tests that directly
  // instantiate a RenderThreadImpl.
  if (MojoShellConnection::Get())
    CreateRenderWidgetWindowTreeClientFactory();
#endif
}

RenderThreadImpl::~RenderThreadImpl() {
}

void RenderThreadImpl::Shutdown() {
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, OnRenderProcessShutdown());

  if (memory_observer_) {
    message_loop()->RemoveTaskObserver(memory_observer_.get());
    memory_observer_.reset();
  }

  // Wait for all databases to be closed.
  if (blink_platform_impl_) {
    // Crash the process if they fail to close after a generous amount of time.
    bool all_closed = blink_platform_impl_->web_database_observer_impl()
        ->WaitForAllDatabasesToClose(base::TimeDelta::FromSeconds(60));
    CHECK(all_closed);
  }

  // Shutdown in reverse of the initialization order.
  if (devtools_agent_message_filter_.get()) {
    RemoveFilter(devtools_agent_message_filter_.get());
    devtools_agent_message_filter_ = NULL;
  }

  RemoveFilter(audio_input_message_filter_.get());
  audio_input_message_filter_ = NULL;

#if defined(ENABLE_WEBRTC)
  RTCPeerConnectionHandler::DestructAllHandlers();
  // |peer_connection_factory_| cannot be deleted until after the main message
  // loop has been destroyed.  This is because there may be pending tasks that
  // hold on to objects produced by the PC factory that depend on threads owned
  // by the PC factory.  Once those tasks have been freed, the factory can be
  // deleted.
#endif
  RemoveFilter(vc_manager_->video_capture_message_filter());
  vc_manager_.reset();

  RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;

  // Shutdown the file thread if it's running.
  if (file_thread_)
    file_thread_->Stop();

  if (compositor_message_filter_.get()) {
    RemoveFilter(compositor_message_filter_.get());
    compositor_message_filter_ = NULL;
  }

#if defined(OS_ANDROID)
  if (sync_compositor_message_filter_) {
    RemoveFilter(sync_compositor_message_filter_.get());
    sync_compositor_message_filter_ = nullptr;
  }
  stream_texture_factory_ = nullptr;
#endif

  media_thread_.reset();

  blink_platform_impl_->SetCompositorThread(nullptr);

  compositor_thread_.reset();

  // AudioMessageFilter may be accessed on |media_thread_|, so shutdown after.
  RemoveFilter(audio_message_filter_.get());
  audio_message_filter_ = NULL;

  raster_worker_pool_->Shutdown();

  main_input_callback_.Cancel();
  input_handler_manager_.reset();
  if (input_event_filter_.get()) {
    RemoveFilter(input_event_filter_.get());
    input_event_filter_ = NULL;
  }

  // RemoveEmbeddedWorkerRoute may be called while deleting
  // EmbeddedWorkerDispatcher. So it must be deleted before deleting
  // RenderThreadImpl.
  embedded_worker_dispatcher_.reset();

  // Ramp down IDB before we ramp down WebKit (and V8), since IDB classes might
  // hold pointers to V8 objects (e.g., via pending requests).
  main_thread_indexed_db_dispatcher_.reset();

  main_thread_compositor_task_runner_ = NULL;

  gpu_factories_.clear();

  // Context providers must be released prior to destroying the GPU channel.
  shared_worker_context_provider_ = nullptr;
  shared_main_thread_contexts_ = nullptr;

  if (gpu_channel_.get())
    gpu_channel_->DestroyChannel();

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  NPChannelBase::CleanupChannels();
#endif

  ChildThreadImpl::Shutdown();

  // Shut down the message loop and the renderer scheduler before shutting down
  // Blink. This prevents a scenario where a pending task in the message loop
  // accesses Blink objects after Blink shuts down.
  renderer_scheduler_->Shutdown();
  if (main_message_loop_)
    main_message_loop_->RunUntilIdle();

  if (blink_platform_impl_) {
    blink_platform_impl_->Shutdown();
    // This must be at the very end of the shutdown sequence.
    // blink::shutdown() must be called after all strong references from
    // Chromium to Blink are cleared.
    blink::shutdown();
  }

  // Delay shutting down DiscardableSharedMemoryManager until blink::shutdown
  // is complete, because blink::shutdown destructs Blink Resources and they
  // may try to unlock their underlying discardable memory.
  ChildThreadImpl::ShutdownDiscardableSharedMemoryManager();

  // The message loop must be cleared after shutting down
  // the DiscardableSharedMemoryManager, which needs to send messages
  // to the browser process.
  main_message_loop_.reset();

  lazy_tls.Pointer()->Set(NULL);
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // Certain synchronous messages cannot always be processed synchronously by
  // the browser, e.g., putting up UI and waiting for the user. This could cause
  // a complete hang of Chrome if a windowed plugin is trying to communicate
  // with the renderer thread since the browser's UI thread could be stuck
  // (within a Windows API call) trying to synchronously communicate with the
  // plugin.  The remedy is to pump messages on this thread while the browser
  // is processing this request. This creates an opportunity for re-entrancy
  // into WebKit, so we need to take care to disable callbacks, timers, and
  // pending network loads that could trigger such callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    }
  }

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

#if defined(ENABLE_PLUGINS)
  int render_view_id = MSG_ROUTING_NONE;
#endif

  if (pumping_events) {
    renderer_scheduler_->SuspendTimerQueue();

    if (notify_webkit_of_modal_loop)
      WebView::willEnterModalLoop();
#if defined(ENABLE_PLUGINS)
    RenderViewImpl* render_view =
        RenderViewImpl::FromRoutingID(msg->routing_id());
    if (render_view) {
      render_view_id = msg->routing_id();
      PluginChannelHost::Broadcast(
          new PluginMsg_SignalModalDialogEvent(render_view_id));
    }
#endif
  }

  bool rv = ChildThreadImpl::Send(msg);

  if (pumping_events) {
#if defined(ENABLE_PLUGINS)
    if (render_view_id != MSG_ROUTING_NONE) {
      PluginChannelHost::Broadcast(
          new PluginMsg_ResetModalDialogEvent(render_view_id));
    }
#endif

    if (notify_webkit_of_modal_loop)
      WebView::didExitModalLoop();

    renderer_scheduler_->ResumeTimerQueue();
  }

  return rv;
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty());
  return lang;
}

IPC::SyncMessageFilter* RenderThreadImpl::GetSyncMessageFilter() {
  return sync_message_filter();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetIOMessageLoopProxy() {
  return ChildProcess::current()->io_task_runner();
}

void RenderThreadImpl::AddRoute(int32_t routing_id, IPC::Listener* listener) {
  ChildThreadImpl::GetRouter()->AddRoute(routing_id, listener);
  PendingRenderFrameConnectMap::iterator it =
      pending_render_frame_connects_.find(routing_id);
  if (it == pending_render_frame_connects_.end())
    return;

  RenderFrameImpl* frame = RenderFrameImpl::FromRoutingID(routing_id);
  if (!frame)
    return;

  scoped_refptr<PendingRenderFrameConnect> connection(it->second);
  mojo::InterfaceRequest<mojo::ServiceProvider> services(
      std::move(connection->services()));
  mojo::ServiceProviderPtr exposed_services(
      std::move(connection->exposed_services()));
  exposed_services.set_connection_error_handler(mojo::Closure());
  pending_render_frame_connects_.erase(it);

  frame->BindServiceRegistry(std::move(services), std::move(exposed_services));
}

void RenderThreadImpl::RemoveRoute(int32_t routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
}

void RenderThreadImpl::AddEmbeddedWorkerRoute(int32_t routing_id,
                                              IPC::Listener* listener) {
  AddRoute(routing_id, listener);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->AddEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RemoveEmbeddedWorkerRoute(int32_t routing_id) {
  RemoveRoute(routing_id);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->RemoveEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RegisterPendingRenderFrameConnect(
    int routing_id,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  std::pair<PendingRenderFrameConnectMap::iterator, bool> result =
      pending_render_frame_connects_.insert(std::make_pair(
          routing_id,
          make_scoped_refptr(new PendingRenderFrameConnect(
              routing_id, std::move(services), std::move(exposed_services)))));
  CHECK(result.second) << "Inserting a duplicate item.";
}

int RenderThreadImpl::GenerateRoutingID() {
  int routing_id = MSG_ROUTING_NONE;
  Send(new ViewHostMsg_GenerateRoutingID(&routing_id));
  return routing_id;
}

void RenderThreadImpl::AddFilter(IPC::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::AddObserver(RenderProcessObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderThreadImpl::RemoveObserver(RenderProcessObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceDispatcherDelegate(
    ResourceDispatcherDelegate* delegate) {
  resource_dispatcher()->set_delegate(delegate);
}

void RenderThreadImpl::SetResourceDispatchTaskQueue(
    const scoped_refptr<base::SingleThreadTaskRunner>& resource_task_queue) {
  // Add a filter that forces resource messages to be dispatched via a
  // particular task runner.
  scoped_refptr<ResourceSchedulingFilter> filter(
      new ResourceSchedulingFilter(resource_task_queue, resource_dispatcher()));
  channel()->AddFilter(filter.get());
  resource_dispatcher()->SetResourceSchedulingFilter(filter);

  // The ChildResourceMessageFilter and the ResourceDispatcher need to use the
  // same queue to ensure tasks are executed in the expected order.
  child_resource_message_filter()->SetMainThreadTaskRunner(resource_task_queue);
  resource_dispatcher()->SetMainThreadTaskRunner(resource_task_queue);
}

void RenderThreadImpl::InitializeCompositorThread() {
#if defined(OS_ANDROID)
  SynchronousCompositorFactory* sync_compositor_factory =
      SynchronousCompositorFactory::GetInstance();
  bool using_ipc_sync_compositing =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIPCSyncCompositing);
  DCHECK(!sync_compositor_factory || !using_ipc_sync_compositing);

  if (sync_compositor_factory) {
    compositor_task_runner_ =
        sync_compositor_factory->GetCompositorTaskRunner();
  }
#endif
  if (!compositor_task_runner_.get()) {
    base::Thread::Options options;
#if defined(OS_ANDROID)
    options.priority = base::ThreadPriority::DISPLAY;
#endif
    compositor_thread_.reset(new WebThreadForCompositor(options));
    blink_platform_impl_->SetCompositorThread(compositor_thread_.get());
    compositor_task_runner_ = compositor_thread_->TaskRunner();
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&ThreadRestrictions::SetIOAllowed),
                   false));
  }

  InputHandlerManagerClient* input_handler_manager_client = NULL;
#if defined(OS_ANDROID)
  if (using_ipc_sync_compositing) {
    sync_compositor_message_filter_ =
        new SynchronousCompositorFilter(compositor_task_runner_);
    AddFilter(sync_compositor_message_filter_.get());
    input_handler_manager_client = sync_compositor_message_filter_.get();
  } else if (sync_compositor_factory) {
    input_handler_manager_client =
        sync_compositor_factory->GetInputHandlerManagerClient();
  }
#endif
  if (!input_handler_manager_client) {
    scoped_refptr<InputEventFilter> compositor_input_event_filter(
        new InputEventFilter(main_input_callback_.callback(),
                             main_thread_compositor_task_runner_,
                             compositor_task_runner_));
    input_handler_manager_client = compositor_input_event_filter.get();
    input_event_filter_ = compositor_input_event_filter;
  }
  input_handler_manager_.reset(new InputHandlerManager(
      compositor_task_runner_, input_handler_manager_client,
      renderer_scheduler_.get()));
}

void RenderThreadImpl::EnsureWebKitInitialized() {
  if (blink_platform_impl_)
    return;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

#ifdef ENABLE_VTUNE_JIT_INTERFACE
  if (command_line.HasSwitch(switches::kEnableVtune))
    gin::Debug::SetJitCodeEventHandler(vTune::GetVtuneCodeEventHandler());
#endif

  SetRuntimeFeaturesDefaultsAndUpdateFromArgs(command_line);

  blink_platform_impl_.reset(
      new RendererBlinkPlatformImpl(renderer_scheduler_.get()));
  blink::initialize(blink_platform_impl_.get());

  v8::Isolate* isolate = blink::mainThreadIsolate();
  isolate->SetCreateHistogramFunction(CreateHistogram);
  isolate->SetAddHistogramSampleFunction(AddHistogramSample);

  main_thread_compositor_task_runner_ =
      renderer_scheduler_->CompositorTaskRunner();

  main_input_callback_.Reset(
      base::Bind(base::IgnoreResult(&RenderThreadImpl::OnMessageReceived),
                 base::Unretained(this)));

  SetResourceDispatchTaskQueue(renderer_scheduler_->LoadingTaskRunner());
  if (!command_line.HasSwitch(switches::kDisableThreadedCompositing) &&
      !command_line.HasSwitch(switches::kUseRemoteCompositing))
    InitializeCompositorThread();

  if (!input_event_filter_.get()) {
    // Always provide an input event filter implementation to ensure consistent
    // input event scheduling and prioritization.
    // TODO(jdduke): Merge InputEventFilter, InputHandlerManager and
    // MainThreadInputEventFilter, crbug.com/436057.
    input_event_filter_ = new MainThreadInputEventFilter(
        main_input_callback_.callback(), main_thread_compositor_task_runner_);
  }
  AddFilter(input_event_filter_.get());

  scoped_refptr<base::SingleThreadTaskRunner> compositor_impl_side_task_runner;
  if (compositor_task_runner_)
    compositor_impl_side_task_runner = compositor_task_runner_;
  else
    compositor_impl_side_task_runner = base::ThreadTaskRunnerHandle::Get();

  compositor_message_filter_ = new CompositorForwardingMessageFilter(
      compositor_impl_side_task_runner.get());
  AddFilter(compositor_message_filter_.get());

  RenderThreadImpl::RegisterSchemes();

  EnableBlinkPlatformLogChannels(
      command_line.GetSwitchValueASCII(switches::kBlinkPlatformLogChannels));

  RenderMediaClient::Initialize();

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, WebKitInitialized());

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  v8_sampling_profiler_.reset(new V8SamplingProfiler());

  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
  } else {
    // If we do not track widget visibility, then assume conservatively that
    // the isolate is in background. This reduces memory usage.
    isolate->IsolateInBackgroundNotification();
  }

  renderer_scheduler_->SetTimerQueueSuspensionWhenBackgroundedEnabled(
      GetContentClient()
          ->renderer()
          ->AllowTimerSuspensionWhenProcessBackgrounded());

  cc_blink::SetSharedBitmapAllocationFunction(AllocateSharedBitmapFunction);

  SkGraphics::SetResourceCacheSingleAllocationByteLimit(
      kImageCacheSingleAllocationByteLimit);

  // Hook up blink's codecs so skia can call them
  SkGraphics::SetImageGeneratorFromEncodedFactory(
      blink::WebImageGenerator::create);

  if (command_line.HasSwitch(switches::kMemoryMetrics)) {
    memory_observer_.reset(new MemoryObserver());
    message_loop()->AddTaskObserver(memory_observer_.get());
  }

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // swappedout:
  WebString swappedout_scheme(base::ASCIIToUTF16(kSwappedOutScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(swappedout_scheme);
  WebSecurityPolicy::registerURLSchemeAsEmptyDocument(swappedout_scheme);

  // chrome:
  WebString chrome_scheme(base::ASCIIToUTF16(kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs(
      chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsSecure(chrome_scheme);
  WebSecurityPolicy::registerURLSchemeAsCORSEnabled(chrome_scheme);

  // chrome-devtools:
  WebString devtools_scheme(base::ASCIIToUTF16(kChromeDevToolsScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(devtools_scheme);
}

void RenderThreadImpl::NotifyTimezoneChange() {
  NotifyTimezoneChangeOnThisThread();
  RenderThread::Get()->PostTaskToAllWebWorkers(
      base::Bind(&NotifyTimezoneChangeOnThisThread));
}

void RenderThreadImpl::RecordAction(const base::UserMetricsAction& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action.str_));
}

void RenderThreadImpl::RecordComputedAction(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

scoped_ptr<base::SharedMemory>
    RenderThreadImpl::HostAllocateSharedMemoryBuffer(size_t size) {
  return ChildThreadImpl::AllocateSharedMemory(size, thread_safe_sender());
}

cc::SharedBitmapManager* RenderThreadImpl::GetSharedBitmapManager() {
  return shared_bitmap_manager();
}

void RenderThreadImpl::RegisterExtension(v8::Extension* extension) {
  EnsureWebKitInitialized();
  WebScriptController::registerExtension(extension);
}

void RenderThreadImpl::ScheduleIdleHandler(int64_t initial_delay_ms) {
  idle_notification_delay_in_ms_ = initial_delay_ms;
  idle_timer_.Stop();
  idle_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(initial_delay_ms),
      this, &RenderThreadImpl::IdleHandler);
}

void RenderThreadImpl::IdleHandler() {
  bool run_in_foreground_tab = (widget_count_ > hidden_widget_count_) &&
                               GetContentClient()->renderer()->
                                   RunIdleHandlerWhenWidgetsHidden();
  if (run_in_foreground_tab) {
    if (idle_notifications_to_skip_ > 0) {
      --idle_notifications_to_skip_;
    } else {
      ReleaseFreeMemory();
    }
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
    return;
  }

  ReleaseFreeMemory();

  // Continue the idle timer if the webkit shared timer is not suspended or
  // something is left to do.
  bool continue_timer = !webkit_shared_timer_suspended_;

  // Schedule next invocation. When the tab is originally hidden, an invocation
  // is scheduled for kInitialIdleHandlerDelayMs in
  // RenderThreadImpl::WidgetHidden in order to race to a minimal heap.
  // After that, idle calls can be much less frequent, so run at a maximum of
  // once every kLongIdleHandlerDelayMs.
  // Dampen the delay using the algorithm (if delay is in seconds):
  //    delay = delay + 1 / (delay + 2)
  // Using floor(delay) has a dampening effect such as:
  //    30s, 30, 30, 31, 31, 31, 31, 32, 32, ...
  // If the delay is in milliseconds, the above formula is equivalent to:
  //    delay_ms / 1000 = delay_ms / 1000 + 1 / (delay_ms / 1000 + 2)
  // which is equivalent to
  //    delay_ms = delay_ms + 1000*1000 / (delay_ms + 2000).
  if (continue_timer) {
    ScheduleIdleHandler(
        std::max(kLongIdleHandlerDelayMs,
                 idle_notification_delay_in_ms_ +
                 1000000 / (idle_notification_delay_in_ms_ + 2000)));

  } else {
    idle_timer_.Stop();
  }

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, IdleNotification());
}

int64_t RenderThreadImpl::GetIdleNotificationDelayInMs() const {
  return idle_notification_delay_in_ms_;
}

void RenderThreadImpl::SetIdleNotificationDelayInMs(
    int64_t idle_notification_delay_in_ms) {
  idle_notification_delay_in_ms_ = idle_notification_delay_in_ms;
}

void RenderThreadImpl::UpdateHistograms(int sequence_number) {
  child_histogram_message_filter()->SendHistograms(sequence_number);
}

int RenderThreadImpl::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return WorkerThreadRegistry::Instance()->PostTaskToAllThreads(closure);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
}

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

media::GpuVideoAcceleratorFactories* RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  if (!gpu_factories_.empty()) {
    scoped_refptr<ContextProviderCommandBuffer> shared_context_provider =
        gpu_factories_.back()->ContextProviderMainThread();
    if (shared_context_provider) {
      cc::ContextProvider::ScopedContextLock lock(
          shared_context_provider.get());
      if (lock.ContextGL()->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
        return gpu_factories_.back();
      } else {
        scoped_refptr<base::SingleThreadTaskRunner> media_task_runner =
            GetMediaThreadTaskRunner();
        media_task_runner->PostTask(
            FROM_HERE,
            base::Bind(
                base::IgnoreResult(
                    &RendererGpuVideoAcceleratorFactories::CheckContextLost),
                base::Unretained(gpu_factories_.back())));
      }
    }
  }

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner =
      GetMediaThreadTaskRunner();
  scoped_refptr<ContextProviderCommandBuffer> shared_context_provider =
      SharedWorkerContextProvider();
  scoped_refptr<GpuChannelHost> gpu_channel_host = GetGpuChannel();
  if (shared_context_provider && gpu_channel_host) {
    const bool enable_video_accelerator =
        !cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode);
    const bool enable_gpu_memory_buffer_video_frames =
#if defined(OS_MACOSX)
        !cmd_line->HasSwitch(switches::kDisableGpuMemoryBufferVideoFrames);
#else
        cmd_line->HasSwitch(switches::kEnableGpuMemoryBufferVideoFrames);
#endif
    std::vector<unsigned> image_texture_targets;
    std::string video_frame_image_texture_target_string =
        cmd_line->GetSwitchValueASCII(switches::kVideoImageTextureTarget);
    StringToUintVector(video_frame_image_texture_target_string,
                       &image_texture_targets);

    gpu_factories_.push_back(RendererGpuVideoAcceleratorFactories::Create(
        gpu_channel_host.get(), base::ThreadTaskRunnerHandle::Get(),
        media_task_runner, shared_context_provider,
        enable_gpu_memory_buffer_video_frames, image_texture_targets,
        enable_video_accelerator));
    return gpu_factories_.back();
  }
  return nullptr;
}

scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
RenderThreadImpl::CreateOffscreenContext3d() {
  blink::WebGraphicsContext3D::Attributes attributes(GetOffscreenAttribs());
  bool lose_context_when_out_of_memory = true;

  scoped_refptr<GpuChannelHost> gpu_channel_host(EstablishGpuChannelSync(
      CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE));
  return make_scoped_ptr(
      WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
          gpu_channel_host.get(),
          attributes,
          lose_context_when_out_of_memory,
          GURL("chrome://gpu/RenderThreadImpl::CreateOffscreenContext3d"),
          WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
          NULL));
}

scoped_refptr<cc_blink::ContextProviderWebContext>
RenderThreadImpl::SharedMainThreadContextProvider() {
  DCHECK(IsMainThread());
  if (!shared_main_thread_contexts_.get() ||
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    shared_main_thread_contexts_ = ContextProviderCommandBuffer::Create(
        CreateOffscreenContext3d(), RENDERER_MAINTHREAD_CONTEXT);
    if (shared_main_thread_contexts_.get() &&
        !shared_main_thread_contexts_->BindToCurrentThread())
      shared_main_thread_contexts_ = NULL;
  }
  return shared_main_thread_contexts_;
}

#if defined(OS_ANDROID)
scoped_refptr<StreamTextureFactory> RenderThreadImpl::GetStreamTexureFactory() {
  DCHECK(IsMainThread());
  if (!stream_texture_factory_.get() ||
      stream_texture_factory_->ContextGL()->GetGraphicsResetStatusKHR() !=
          GL_NO_ERROR) {
    if (!SharedMainThreadContextProvider().get()) {
      stream_texture_factory_ = NULL;
      return NULL;
    }
    scoped_refptr<GpuChannelHost> gpu_channel_host(EstablishGpuChannelSync(
        CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE));
    if (!gpu_channel_host.get()) {
      LOG(ERROR) << "Failed to establish GPU channel for media player";
      stream_texture_factory_ = NULL;
    } else {
      stream_texture_factory_ = StreamTextureFactoryImpl::Create(
          shared_main_thread_contexts_, gpu_channel_host.get());
    }
  }
  return stream_texture_factory_;
}
#endif

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_) {
    audio_renderer_mixer_manager_.reset(new AudioRendererMixerManager());
  }

  return audio_renderer_mixer_manager_.get();
}

media::AudioHardwareConfig* RenderThreadImpl::GetAudioHardwareConfig() {
  if (!audio_hardware_config_) {
    media::AudioParameters input_params;
    media::AudioParameters output_params;
    Send(new ViewHostMsg_GetAudioHardwareConfig(
        &input_params, &output_params));

    audio_hardware_config_.reset(new media::AudioHardwareConfig(
        input_params, output_params));
  }

  return audio_hardware_config_.get();
}

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

#if defined(OS_WIN)
void RenderThreadImpl::PreCacheFontCharacters(const LOGFONT& log_font,
                                              const base::string16& str) {
  Send(new RenderProcessHostMsg_PreCacheFontCharacters(log_font, str));
}

#endif  // OS_WIN

ServiceRegistry* RenderThreadImpl::GetServiceRegistry() {
  return service_registry();
}

bool RenderThreadImpl::IsGpuRasterizationForced() {
  return is_gpu_rasterization_forced_;
}

bool RenderThreadImpl::IsGpuRasterizationEnabled() {
  return is_gpu_rasterization_enabled_;
}

int RenderThreadImpl::GetGpuRasterizationMSAASampleCount() {
  return gpu_rasterization_msaa_sample_count_;
}

bool RenderThreadImpl::IsLcdTextEnabled() {
  return is_lcd_text_enabled_;
}

bool RenderThreadImpl::IsDistanceFieldTextEnabled() {
  return is_distance_field_text_enabled_;
}

bool RenderThreadImpl::IsZeroCopyEnabled() {
  return is_zero_copy_enabled_;
}

bool RenderThreadImpl::IsPartialRasterEnabled() {
  return is_partial_raster_enabled_;
}

bool RenderThreadImpl::IsGpuMemoryBufferCompositorResourcesEnabled() {
  return is_gpu_memory_buffer_compositor_resources_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

std::vector<unsigned> RenderThreadImpl::GetImageTextureTargets() {
  return use_image_texture_targets_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorMainThreadTaskRunner() {
  return main_thread_compositor_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorImplThreadTaskRunner() {
  return compositor_task_runner_;
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager();
}

scheduler::RendererScheduler* RenderThreadImpl::GetRendererScheduler() {
  return renderer_scheduler_.get();
}

cc::ContextProvider* RenderThreadImpl::GetSharedMainThreadContextProvider() {
  return SharedMainThreadContextProvider().get();
}

scoped_ptr<cc::BeginFrameSource>
RenderThreadImpl::CreateExternalBeginFrameSource(int routing_id) {
#if defined(OS_ANDROID)
  if (SynchronousCompositorFactory* factory =
          SynchronousCompositorFactory::GetInstance()) {
    DCHECK(!sync_compositor_message_filter_);
    return factory->CreateExternalBeginFrameSource(routing_id);
  } else if (sync_compositor_message_filter_) {
    return make_scoped_ptr(new SynchronousCompositorExternalBeginFrameSource(
        routing_id, sync_compositor_message_filter_.get()));
  }
#endif
  return make_scoped_ptr(new CompositorExternalBeginFrameSource(
      compositor_message_filter_.get(), sync_message_filter(), routing_id));
}

cc::TaskGraphRunner* RenderThreadImpl::GetTaskGraphRunner() {
  return raster_worker_pool_->GetTaskGraphRunner();
}

bool RenderThreadImpl::AreImageDecodeTasksEnabled() {
  return are_image_decode_tasks_enabled_;
}

bool RenderThreadImpl::IsThreadedAnimationEnabled() {
  return is_threaded_animation_enabled_;
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetIOThreadTaskRunner() {
  return io_thread_task_runner_;
}

scoped_ptr<base::SharedMemory> RenderThreadImpl::AllocateSharedMemory(
    size_t size) {
  return HostAllocateSharedMemoryBuffer(size);
}

CreateCommandBufferResult RenderThreadImpl::CreateViewCommandBuffer(
    int32_t surface_id,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id) {
  NOTREACHED();
  return CREATE_COMMAND_BUFFER_FAILED;
}

void RenderThreadImpl::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  base::ObserverListBase<RenderProcessObserver>::Iterator it(&observers_);
  RenderProcessObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnControlMessageReceived(msg))
      return true;
  }

  // Some messages are handled by delegates.
  if (appcache_dispatcher_->OnMessageReceived(msg) ||
      dom_storage_dispatcher_->OnMessageReceived(msg) ||
      embedded_worker_dispatcher_->OnMessageReceived(msg)) {
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderThreadImpl, msg)
    IPC_MESSAGE_HANDLER(FrameMsg_NewFrame, OnCreateNewFrame)
    IPC_MESSAGE_HANDLER(FrameMsg_NewFrameProxy, OnCreateNewFrameProxy)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForCurrentURL,
                        OnSetZoomLevelForCurrentURL)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_NetworkConnectionChanged,
                        OnNetworkConnectionChanged)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateNewSharedWorker)
    IPC_MESSAGE_HANDLER(ViewMsg_TimezoneChange, OnUpdateTimezone)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWebKitSharedTimersSuspended,
                        OnSetWebKitSharedTimersSuspended)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScrollbarTheme, OnUpdateScrollbarTheme)
    IPC_MESSAGE_HANDLER(ViewMsg_SystemColorsChanged, OnSystemColorsChanged)
#endif
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache, OnPurgePluginListCache)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnProcessBackgrounded(bool backgrounded) {
  ChildThreadImpl::OnProcessBackgrounded(backgrounded);

  if (backgrounded)
    renderer_scheduler_->OnRendererBackgrounded();
  else
    renderer_scheduler_->OnRendererForegrounded();
}

void RenderThreadImpl::OnCreateNewFrame(FrameMsg_NewFrame_Params params) {
  CompositorDependencies* compositor_deps = this;
  RenderFrameImpl::CreateFrame(
      params.routing_id, params.proxy_routing_id, params.opener_routing_id,
      params.parent_routing_id, params.previous_sibling_routing_id,
      params.replication_state, compositor_deps, params.widget_params,
      params.frame_owner_properties);
}

void RenderThreadImpl::OnCreateNewFrameProxy(
    int routing_id,
    int render_view_routing_id,
    int opener_routing_id,
    int parent_routing_id,
    const FrameReplicationState& replicated_state) {
  // Debug cases of https://crbug.com/575245.
  base::debug::SetCrashKeyValue("newproxy_proxy_id",
                                base::IntToString(routing_id));
  base::debug::SetCrashKeyValue("newproxy_view_id",
                                base::IntToString(render_view_routing_id));
  base::debug::SetCrashKeyValue("newproxy_opener_id",
                                base::IntToString(opener_routing_id));
  base::debug::SetCrashKeyValue("newproxy_parent_id",
                                base::IntToString(parent_routing_id));
  RenderFrameProxy::CreateFrameProxy(routing_id, render_view_routing_id,
                                     opener_routing_id, parent_routing_id,
                                     replicated_state);
}

void RenderThreadImpl::OnSetZoomLevelForCurrentURL(const std::string& scheme,
                                                   const std::string& host,
                                                   double zoom_level) {
  RenderViewZoomer zoomer(scheme, host, zoom_level);
  RenderView::ForEach(&zoomer);
}

void RenderThreadImpl::OnCreateNewView(const ViewMsg_New_Params& params) {
  EnsureWebKitInitialized();
  CompositorDependencies* compositor_deps = this;
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(compositor_deps, params, false);
}

GpuChannelHost* RenderThreadImpl::EstablishGpuChannelSync(
    CauseForGpuLaunch cause_for_gpu_launch) {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  if (gpu_channel_.get()) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (!gpu_channel_->IsLost())
      return gpu_channel_.get();

    // Recreate the channel if it has been lost.
    gpu_channel_->DestroyChannel();
    gpu_channel_ = NULL;
  }

  // Ask the browser for the channel name.
  int client_id = 0;
  IPC::ChannelHandle channel_handle;
  gpu::GPUInfo gpu_info;
  if (!Send(new GpuHostMsg_EstablishGpuChannel(cause_for_gpu_launch,
                                               &client_id,
                                               &channel_handle,
                                               &gpu_info)) ||
#if defined(OS_POSIX)
      channel_handle.socket.fd == -1 ||
#endif
      channel_handle.name.empty()) {
    // Otherwise cancel the connection.
    return NULL;
  }

  GetContentClient()->SetGpuInfo(gpu_info);

  // Cache some variables that are needed on the compositor thread for our
  // implementation of GpuChannelHostFactory.
  io_thread_task_runner_ = ChildProcess::current()->io_task_runner();

  gpu_channel_ =
      GpuChannelHost::Create(this,
                             client_id,
                             gpu_info,
                             channel_handle,
                             ChildProcess::current()->GetShutDownEvent(),
                             gpu_memory_buffer_manager());
  return gpu_channel_.get();
}

blink::WebMediaStreamCenter* RenderThreadImpl::CreateMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
#if defined(ENABLE_WEBRTC)
  if (!media_stream_center_) {
    media_stream_center_ = GetContentClient()->renderer()
        ->OverrideCreateWebMediaStreamCenter(client);
    if (!media_stream_center_) {
      scoped_ptr<MediaStreamCenter> media_stream_center(
          new MediaStreamCenter(client, GetPeerConnectionDependencyFactory()));
      media_stream_center_ = media_stream_center.release();
    }
  }
#endif
  return media_stream_center_;
}

#if defined(ENABLE_WEBRTC)
PeerConnectionDependencyFactory*
RenderThreadImpl::GetPeerConnectionDependencyFactory() {
  return peer_connection_factory_.get();
}
#endif

GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  if (!gpu_channel_.get())
    return NULL;

  if (gpu_channel_->IsLost())
    return NULL;

  return gpu_channel_.get();
}

#if defined(ENABLE_PLUGINS)
void RenderThreadImpl::OnPurgePluginListCache(bool reload_pages) {
  EnsureWebKitInitialized();
  // The call below will cause a GetPlugins call with refresh=true, but at this
  // point we already know that the browser has refreshed its list, so disable
  // refresh temporarily to prevent each renderer process causing the list to be
  // regenerated.
  blink_platform_impl_->set_plugin_refresh_allowed(false);
  blink::resetPluginCache(reload_pages);
  blink_platform_impl_->set_plugin_refresh_allowed(true);

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, PluginListChanged());
}
#endif

void RenderThreadImpl::OnNetworkConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType type,
    double max_bandwidth_mbps) {
  EnsureWebKitInitialized();
  bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::setOnLine(online);
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, NetworkStateChanged(online));
  WebNetworkStateNotifier::setWebConnection(
      NetConnectionTypeToWebConnectionType(type), max_bandwidth_mbps);
}

void RenderThreadImpl::OnUpdateTimezone(const std::string& zone_id) {
  if (!blink_platform_impl_)
    return;
  if (!zone_id.empty()) {
    icu::TimeZone *new_zone = icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(zone_id));
    icu::TimeZone::adoptDefault(new_zone);
    VLOG(1) << "ICU default timezone is set to " << zone_id;
  }
  NotifyTimezoneChange();
}

#if defined(OS_ANDROID)
void RenderThreadImpl::OnSetWebKitSharedTimersSuspended(bool suspend) {
  EnsureWebKitInitialized();
  if (suspend) {
    renderer_scheduler_->SuspendTimerQueue();
  } else {
    renderer_scheduler_->ResumeTimerQueue();
  }
  webkit_shared_timer_suspended_ = suspend;
}
#endif

#if defined(OS_MACOSX)
void RenderThreadImpl::OnUpdateScrollbarTheme(
    const ViewMsg_UpdateScrollbarTheme_Params& params) {
  EnsureWebKitInitialized();
  static_cast<WebScrollbarBehaviorImpl*>(
      blink_platform_impl_->scrollbarBehavior())
      ->set_jump_on_track_click(params.jump_on_track_click);

  blink::WebScrollbarTheme::updateScrollbarsWithNSDefaults(
      params.initial_button_delay, params.autoscroll_button_delay,
      params.preferred_scroller_style, params.redraw,
      params.scroll_animation_enabled, params.button_placement);
}

void RenderThreadImpl::OnSystemColorsChanged(
    int aqua_color_variant,
    const std::string& highlight_text_color,
    const std::string& highlight_color) {
  SystemColorsDidChange(aqua_color_variant, highlight_text_color,
                        highlight_color);
}
#endif

void RenderThreadImpl::OnCreateNewSharedWorker(
    const WorkerProcessMsg_CreateWorker_Params& params) {
  // EmbeddedSharedWorkerStub will self-destruct.
  new EmbeddedSharedWorkerStub(params.url,
                               params.name,
                               params.content_security_policy,
                               params.security_policy_type,
                               params.pause_on_start,
                               params.route_id);
}

void RenderThreadImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  ReleaseFreeMemory();

  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    blink::WebMemoryPressureListener::onMemoryPressure(
        static_cast<blink::WebMemoryPressureLevel>(memory_pressure_level));

    if (blink::mainThreadIsolate()) {
      // Trigger full v8 garbage collection on memory pressure notifications.
      // This will potentially hang the renderer for a long time, however, when
      // we receive a memory pressure notification, we might be about to be
      // killed.
      blink::mainThreadIsolate()->LowMemoryNotification();
      RenderThread::Get()->PostTaskToAllWebWorkers(
          base::Bind(&LowMemoryNotificationOnThisThread));
    }

    if (memory_pressure_level ==
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      // Purge Skia font cache, by setting it to 0 and then again to the
      // previous limit.
      size_t font_cache_limit = SkGraphics::SetFontCacheLimit(0);
      SkGraphics::SetFontCacheLimit(font_cache_limit);
    }
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetFileThreadMessageLoopProxy() {
  DCHECK(message_loop() == base::MessageLoop::current());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetMediaThreadTaskRunner() {
  DCHECK(message_loop() == base::MessageLoop::current());
  if (!media_thread_) {
    media_thread_.reset(new base::Thread("Media"));
    media_thread_->Start();

#if defined(OS_ANDROID)
    renderer_demuxer_ = new RendererDemuxerAndroid();
    AddFilter(renderer_demuxer_.get());
#endif
  }
  return media_thread_->task_runner();
}

base::TaskRunner* RenderThreadImpl::GetWorkerTaskRunner() {
  return raster_worker_pool_.get();
}

scoped_refptr<ContextProviderCommandBuffer>
RenderThreadImpl::SharedWorkerContextProvider() {
  DCHECK(IsMainThread());
  // Try to reuse existing shared worker context provider.
  bool shared_worker_context_provider_lost = false;
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    base::AutoLock lock(*shared_worker_context_provider_->GetLock());
    if (shared_worker_context_provider_->ContextGL()
            ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      shared_worker_context_provider_lost = true;
    }
  }
  if (!shared_worker_context_provider_ || shared_worker_context_provider_lost) {
    shared_worker_context_provider_ = ContextProviderCommandBuffer::Create(
        CreateOffscreenContext3d(), RENDER_WORKER_CONTEXT);
    if (shared_worker_context_provider_ &&
        !shared_worker_context_provider_->BindToCurrentThread())
      shared_worker_context_provider_ = nullptr;
    if (shared_worker_context_provider_)
      shared_worker_context_provider_->SetupLock();
  }
  return shared_worker_context_provider_;
}

void RenderThreadImpl::SampleGamepads(blink::WebGamepads* data) {
  blink_platform_impl_->sampleGamepads(*data);
}

bool RenderThreadImpl::RendererIsHidden() const {
  return widget_count_ > 0 && hidden_widget_count_ == widget_count_;
}

void RenderThreadImpl::WidgetCreated() {
  bool renderer_was_hidden = RendererIsHidden();
  widget_count_++;
  if (renderer_was_hidden)
    OnRendererVisible();
}

void RenderThreadImpl::WidgetDestroyed() {
  // TODO(rmcilroy): Remove the restriction that destroyed widgets must be
  // unhidden before WidgetDestroyed is called.
  DCHECK_GT(widget_count_, 0);
  DCHECK_GT(widget_count_, hidden_widget_count_);
  widget_count_--;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK_LT(hidden_widget_count_, widget_count_);
  hidden_widget_count_++;
  if (RendererIsHidden())
    OnRendererHidden();
}

void RenderThreadImpl::WidgetRestored() {
  bool renderer_was_hidden = RendererIsHidden();
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (renderer_was_hidden)
    OnRendererVisible();
}

void RenderThreadImpl::OnRendererHidden() {
  // TODO(rmcilroy): Remove IdleHandler and replace it with an IdleTask
  // scheduled by the RendererScheduler - http://crbug.com/469210.
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
  blink::mainThreadIsolate()->IsolateInBackgroundNotification();
}

void RenderThreadImpl::OnRendererVisible() {
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    return;
  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
  blink::mainThreadIsolate()->IsolateInForegroundNotification();
}

void RenderThreadImpl::ReleaseFreeMemory() {
  base::allocator::ReleaseFreeMemory();
  discardable_shared_memory_manager()->ReleaseFreeMemory();

  if (blink_platform_impl_)
    blink::decommitFreeableMemory();
}

RenderThreadImpl::PendingRenderFrameConnect::PendingRenderFrameConnect(
    int routing_id,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services)
    : routing_id_(routing_id),
      services_(std::move(services)),
      exposed_services_(std::move(exposed_services)) {
  // The RenderFrame may be deleted before the ExchangeServiceProviders message
  // is received. In that case, the RenderFrameHost should close the connection,
  // which is detected by setting an error handler on |exposed_services_|.
  exposed_services_.set_connection_error_handler(base::Bind(
      &RenderThreadImpl::PendingRenderFrameConnect::OnConnectionError,
      base::Unretained(this)));
}

RenderThreadImpl::PendingRenderFrameConnect::~PendingRenderFrameConnect() {
}

void RenderThreadImpl::PendingRenderFrameConnect::OnConnectionError() {
  size_t erased =
      RenderThreadImpl::current()->pending_render_frame_connects_.erase(
          routing_id_);
  DCHECK_EQ(1u, erased);
}

}  // namespace content
