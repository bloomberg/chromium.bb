// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_shmem_allocator.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "cc/blink/web_external_bitmap_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/resources/tile_task_worker_pool.h"
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
#include "content/child/worker_task_runner.h"
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
#include "content/common/resource_messages.h"
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
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
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
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/scheduler/renderer_scheduler.h"
#include "content/renderer/scheduler/resource_dispatch_throttler.h"
#include "content/renderer/service_worker/embedded_worker_context_message_filter.h"
#include "content/renderer/service_worker/embedded_worker_dispatcher.h"
#include "content/renderer/shared_worker/embedded_shared_worker_stub.h"
#include "gin/public/debug.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/mojo/ipc_channel_mojo.h"
#include "media/base/audio_hardware_config.h"
#include "media/base/media.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "mojo/common/common_type_converters.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "skia/ext/event_tracer_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebColorName.h"
#include "third_party/WebKit/public/web/WebDatabase.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNetworkStateNotifier.h"
#include "third_party/WebKit/public/web/WebPopupMenu.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebScriptController.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "v8/include/v8.h"

#if defined(OS_ANDROID)
#include <cpu-features.h>
#include "content/renderer/android/synchronous_compositor_factory.h"
#include "content/renderer/media/android/renderer_demuxer_android.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
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

using base::ThreadRestrictions;
using blink::WebDocument;
using blink::WebFrame;
using blink::WebNetworkStateNotifier;
using blink::WebRuntimeFeatures;
using blink::WebScriptController;
using blink::WebSecurityPolicy;
using blink::WebString;
using blink::WebView;

namespace content {

namespace {

const int64 kInitialIdleHandlerDelayMs = 1000;
const int64 kLongIdleHandlerDelayMs = 30*1000;

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

const size_t kEmulatedDiscardableMemoryBytesToKeepWhenWidgetsHidden =
    4 * 1024 * 1024;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

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
    WebDocument document = webview->mainFrame()->document();

    // Don't set zoom level for full-page plugin since they don't use the same
    // zoom settings.
    if (document.isPluginDocument())
      return true;
    GURL url(document.url());
    // Empty scheme works as wildcard that matches any scheme,
    if ((net::GetHostOrSpecFromURL(url) == host_) &&
        (scheme_.empty() || scheme_ == url.scheme()) &&
        !static_cast<RenderViewImpl*>(render_view)
             ->uses_temporary_zoom_level()) {
      webview->hidePopups();
      webview->setZoomLevel(zoom_level_);
    }
    return true;
  }

 private:
  const std::string scheme_;
  const std::string host_;
  const double zoom_level_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewZoomer);
};

std::string HostToCustomHistogramSuffix(const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  return std::string();
}

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

class RenderFrameSetupImpl : public mojo::InterfaceImpl<RenderFrameSetup> {
 public:
  RenderFrameSetupImpl()
      : routing_id_highmark_(-1) {
  }

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
          frame_routing_id, services.Pass(), exposed_services.Pass());
      return;
    }

    frame->BindServiceRegistry(services.Pass(), exposed_services.Pass());
  }

 private:
  int32_t routing_id_highmark_;
};

void CreateRenderFrameSetup(mojo::InterfaceRequest<RenderFrameSetup> request) {
  mojo::BindToRequest(new RenderFrameSetupImpl(), &request);
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

RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThreadImpl::RenderThreadImpl()
    : ChildThreadImpl(Options(ShouldUseMojoChannel())) {
  Init();
}

RenderThreadImpl::RenderThreadImpl(const std::string& channel_name)
    : ChildThreadImpl(Options(channel_name, ShouldUseMojoChannel())) {
  Init();
}

RenderThreadImpl::RenderThreadImpl(
    scoped_ptr<base::MessageLoop> main_message_loop)
    : ChildThreadImpl(Options(ShouldUseMojoChannel())),
      main_message_loop_(main_message_loop.Pass()) {
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT_BEGIN_ETW("RenderThreadImpl::Init", 0, "");

  base::trace_event::TraceLog::GetInstance()->SetThreadSortIndex(
      base::PlatformThread::CurrentId(),
      kTraceEventRendererMainThreadSortIndex);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // On Mac and Android, the select popups are rendered by the browser.
  blink::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  // In single process the single process is all there is.
  suspend_webkit_shared_timer_ = true;
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
  renderer_scheduler_ = RendererScheduler::Create();
  channel()->SetListenerTaskRunner(renderer_scheduler_->DefaultTaskRunner());
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

  aec_dump_message_filter_ =
      new AecDumpMessageFilter(GetIOMessageLoopProxy(),
                               message_loop()->message_loop_proxy());
  AddFilter(aec_dump_message_filter_.get());

  peer_connection_factory_.reset(new PeerConnectionDependencyFactory(
      p2p_socket_dispatcher_.get()));
#endif  // defined(ENABLE_WEBRTC)

  audio_input_message_filter_ =
      new AudioInputMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter(GetIOMessageLoopProxy());
  AddFilter(audio_message_filter_.get());

  midi_message_filter_ = new MidiMessageFilter(GetIOMessageLoopProxy());
  AddFilter(midi_message_filter_.get());

  AddFilter((new IndexedDBMessageFilter(thread_safe_sender()))->GetFilter());

  AddFilter((new EmbeddedWorkerContextMessageFilter())->GetFilter());

  GetContentClient()->renderer()->RenderThreadStarted();

  InitSkiaEventTracer();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  is_impl_side_painting_enabled_ =
      !command_line.HasSwitch(switches::kDisableImplSidePainting);
  cc_blink::WebLayerImpl::SetImplSidePaintingEnabled(
      is_impl_side_painting_enabled_);

  is_zero_copy_enabled_ = command_line.HasSwitch(switches::kEnableZeroCopy);
  is_one_copy_enabled_ = !command_line.HasSwitch(switches::kDisableOneCopy);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  is_elastic_overscroll_enabled_ =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableThreadedEventHandlingMac) &&
      base::mac::IsOSLionOrLater();
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
      command_line.GetSwitchValueASCII(switches::kUseImageTextureTarget);
  bool parsed_image_texture_target = base::StringToUint(
      image_texture_target_string, &use_image_texture_target_);
  DCHECK(parsed_image_texture_target);

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
    gpu_rasterization_msaa_sample_count_ = 0;
  }
  is_threaded_gpu_rasterization_enabled_ =
      command_line.HasSwitch(switches::kEnableThreadedGpuRasterization);

  if (command_line.HasSwitch(switches::kDisableDistanceFieldText)) {
    is_distance_field_text_enabled_ = false;
  } else if (command_line.HasSwitch(switches::kEnableDistanceFieldText)) {
    is_distance_field_text_enabled_ = true;
  } else {
    is_distance_field_text_enabled_ = false;
  }

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  base::FilePath media_path;
  PathService::Get(DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&RenderThreadImpl::OnMemoryPressure, base::Unretained(this))));

  std::vector<base::DiscardableMemoryType> supported_types;
  base::DiscardableMemory::GetSupportedTypes(&supported_types);
  DCHECK(!supported_types.empty());

  // The default preferred type is always the first one in list.
  base::DiscardableMemoryType type = supported_types[0];

  if (command_line.HasSwitch(switches::kUseDiscardableMemory)) {
    std::string requested_type_name = command_line.GetSwitchValueASCII(
        switches::kUseDiscardableMemory);
    base::DiscardableMemoryType requested_type =
        base::DiscardableMemory::GetNamedType(requested_type_name);
    if (std::find(supported_types.begin(),
                  supported_types.end(),
                  requested_type) != supported_types.end()) {
      type = requested_type;
    } else {
      LOG(ERROR) << "Requested discardable memory type is not supported.";
    }
  }

  base::DiscardableMemory::SetPreferredType(type);

  if (is_impl_side_painting_enabled_) {
    int num_raster_threads = 0;
    std::string string_value =
        command_line.GetSwitchValueASCII(switches::kNumRasterThreads);
    bool parsed_num_raster_threads =
        base::StringToInt(string_value, &num_raster_threads);
    DCHECK(parsed_num_raster_threads) << string_value;
    DCHECK_GT(num_raster_threads, 0);

    // Force maximum 1 thread for threaded GPU rasterization.
    // TODO(vmiura): crbug.com/459760 Support existence of multiple raster
    // threads in GPU raster mode.
    if (is_threaded_gpu_rasterization_enabled_)
      num_raster_threads = 1;

    // In single process, browser compositor already initialized and set up
    // worker threads, can't change the number later for the renderer compistor
    // in the same process.
    if (!command_line.HasSwitch(switches::kSingleProcess))
      cc::TileTaskWorkerPool::SetNumWorkerThreads(num_raster_threads);

#if defined(OS_ANDROID) || defined(OS_LINUX)
    if (!command_line.HasSwitch(
            switches::kUseNormalPriorityForTileTaskWorkerThreads)) {
      cc::TileTaskWorkerPool::SetWorkerThreadPriority(
          base::kThreadPriority_Background);
    }
#endif
  }

  base::DiscardableMemoryShmemAllocator::SetInstance(
      ChildThreadImpl::discardable_shared_memory_manager());

  service_registry()->AddService<RenderFrameSetup>(
      base::Bind(CreateRenderFrameSetup));

  TRACE_EVENT_END_ETW("RenderThreadImpl::Init", 0, "");
}

RenderThreadImpl::~RenderThreadImpl() {
}

void RenderThreadImpl::Shutdown() {
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, OnRenderProcessShutdown());

  ChildThreadImpl::Shutdown();

  if (memory_observer_) {
    message_loop()->RemoveTaskObserver(memory_observer_.get());
    memory_observer_.reset();
  }

  // Wait for all databases to be closed.
  if (blink_platform_impl_) {
    // WaitForAllDatabasesToClose might run a nested message loop. To avoid
    // processing timer events while we're already in the process of shutting
    // down blink, put a ScopePageLoadDeferrer on the stack.
    WebView::willEnterModalLoop();
    blink_platform_impl_->web_database_observer_impl()
        ->WaitForAllDatabasesToClose();
    WebView::didExitModalLoop();
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

  media_thread_.reset();

  // AudioMessageFilter may be accessed on |media_thread_|, so shutdown after.
  RemoveFilter(audio_message_filter_.get());
  audio_message_filter_ = NULL;

  compositor_thread_.reset();

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

  if (gpu_channel_.get())
    gpu_channel_->DestroyChannel();

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  NPChannelBase::CleanupChannels();
#endif

  // Shut down the message loop before shutting down Blink.
  // This prevents a scenario where a pending task in the message loop accesses
  // Blink objects after Blink shuts down.
  // This must be at the very end of the shutdown sequence. You must not touch
  // the message loop after this.
  main_message_loop_.reset();
  if (blink_platform_impl_)
    blink::shutdown();

  lazy_tls.Pointer()->Set(NULL);
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // Certain synchronous messages cannot always be processed synchronously by
  // the browser, e.g., putting up UI and waiting for the user. This could cause
  // a complete hang of Chrome if a windowed plug-in is trying to communicate
  // with the renderer thread since the browser's UI thread could be stuck
  // (within a Windows API call) trying to synchronously communicate with the
  // plug-in.  The remedy is to pump messages on this thread while the browser
  // is processing this request. This creates an opportunity for re-entrancy
  // into WebKit, so we need to take care to disable callbacks, timers, and
  // pending network loads that could trigger such callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    }
  }

  bool suspend_webkit_shared_timer = true;  // default value
  std::swap(suspend_webkit_shared_timer, suspend_webkit_shared_timer_);

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

#if defined(ENABLE_PLUGINS)
  int render_view_id = MSG_ROUTING_NONE;
#endif

  if (pumping_events) {
    if (suspend_webkit_shared_timer)
      blink_platform_impl_->SuspendSharedTimer();

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

    if (suspend_webkit_shared_timer)
      blink_platform_impl_->ResumeSharedTimer();
  }

  return rv;
}

scoped_refptr<base::SingleThreadTaskRunner> RenderThreadImpl::GetTaskRunner() {
  return GetRendererScheduler()->DefaultTaskRunner();
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

scoped_refptr<base::MessageLoopProxy>
    RenderThreadImpl::GetIOMessageLoopProxy() {
  return ChildProcess::current()->io_message_loop_proxy();
}

void RenderThreadImpl::AddRoute(int32 routing_id, IPC::Listener* listener) {
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
      connection->services.Pass());
  mojo::ServiceProviderPtr exposed_services(
      connection->exposed_services.Pass());
  pending_render_frame_connects_.erase(it);

  frame->BindServiceRegistry(services.Pass(), exposed_services.Pass());
}

void RenderThreadImpl::RemoveRoute(int32 routing_id) {
  ChildThreadImpl::GetRouter()->RemoveRoute(routing_id);
}

void RenderThreadImpl::AddEmbeddedWorkerRoute(int32 routing_id,
                                              IPC::Listener* listener) {
  AddRoute(routing_id, listener);
  if (devtools_agent_message_filter_.get()) {
    devtools_agent_message_filter_->AddEmbeddedWorkerRouteOnMainThread(
        routing_id);
  }
}

void RenderThreadImpl::RemoveEmbeddedWorkerRoute(int32 routing_id) {
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
              services.Pass(),
              exposed_services.Pass()))));
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
  resource_scheduling_filter_ =
      new ResourceSchedulingFilter(resource_task_queue, resource_dispatcher());
  channel()->AddFilter(resource_scheduling_filter_.get());

  // The ChildResourceMessageFilter and the ResourceDispatcher need to use the
  // same queue to ensure tasks are executed in the expected order.
  child_resource_message_filter()->SetMainThreadTaskRunner(resource_task_queue);
  resource_dispatcher()->SetMainThreadTaskRunner(resource_task_queue);
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

  bool enable = !command_line.HasSwitch(switches::kDisableThreadedCompositing);
  if (enable) {
#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
        SynchronousCompositorFactory::GetInstance())
      compositor_message_loop_proxy_ =
          factory->GetCompositorMessageLoop();
#endif
    if (!compositor_message_loop_proxy_.get()) {
      compositor_thread_.reset(new base::Thread("Compositor"));
      compositor_thread_->Start();
#if defined(OS_ANDROID)
      compositor_thread_->SetPriority(base::kThreadPriority_Display);
#endif
      compositor_message_loop_proxy_ =
          compositor_thread_->message_loop_proxy();
      compositor_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&ThreadRestrictions::SetIOAllowed),
                     false));
    }

    InputHandlerManagerClient* input_handler_manager_client = NULL;
#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
        SynchronousCompositorFactory::GetInstance()) {
      input_handler_manager_client = factory->GetInputHandlerManagerClient();
    }
#endif
    if (!input_handler_manager_client) {
      scoped_refptr<InputEventFilter> compositor_input_event_filter(
          new InputEventFilter(main_input_callback_.callback(),
                               main_thread_compositor_task_runner_,
                               compositor_message_loop_proxy_));
      input_handler_manager_client = compositor_input_event_filter.get();
      input_event_filter_ = compositor_input_event_filter;
    }
    input_handler_manager_.reset(new InputHandlerManager(
        compositor_message_loop_proxy_, input_handler_manager_client,
        renderer_scheduler_.get()));
  }

  if (!input_event_filter_.get()) {
    // Always provide an input event filter implementation to ensure consistent
    // input event scheduling and prioritization.
    // TODO(jdduke): Merge InputEventFilter, InputHandlerManager and
    // MainThreadInputEventFilter, crbug.com/436057.
    input_event_filter_ = new MainThreadInputEventFilter(
        main_input_callback_.callback(), main_thread_compositor_task_runner_);
  }
  AddFilter(input_event_filter_.get());

  scoped_refptr<base::MessageLoopProxy> compositor_impl_side_loop;
  if (enable)
    compositor_impl_side_loop = compositor_message_loop_proxy_;
  else
    compositor_impl_side_loop = base::MessageLoopProxy::current();

  compositor_message_filter_ = new CompositorForwardingMessageFilter(
      compositor_impl_side_loop.get());
  AddFilter(compositor_message_filter_.get());

  RenderThreadImpl::RegisterSchemes();

  EnableBlinkPlatformLogChannels(
      command_line.GetSwitchValueASCII(switches::kBlinkPlatformLogChannels));

  if (!media::IsMediaLibraryInitialized()) {
    WebRuntimeFeatures::enableWebAudio(false);
  }

  RenderMediaClient::Initialize();

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, WebKitInitialized());

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  v8_sampling_profiler_.reset(new V8SamplingProfiler());

  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);

  cc_blink::SetSharedBitmapAllocationFunction(AllocateSharedBitmapFunction);

  // Limit use of the scaled image cache to when deferred image decoding is
  // enabled.
  if (!command_line.HasSwitch(switches::kEnableDeferredImageDecoding) &&
      !is_impl_side_painting_enabled_)
    SkGraphics::SetResourceCacheTotalByteLimit(0u);

  SkGraphics::SetResourceCacheSingleAllocationByteLimit(
      kImageCacheSingleAllocationByteLimit);

  if (command_line.HasSwitch(switches::kMemoryMetrics)) {
    memory_observer_.reset(new MemoryObserver());
    message_loop()->AddTaskObserver(memory_observer_.get());
  }
}

void RenderThreadImpl::RegisterSchemes() {
  // swappedout: pages should not be accessible, and should also
  // be treated as empty documents that can commit synchronously.
  WebString swappedout_scheme(base::ASCIIToUTF16(kSwappedOutScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(swappedout_scheme);
  WebSecurityPolicy::registerURLSchemeAsEmptyDocument(swappedout_scheme);
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
  WebScriptController::registerExtension(extension);
}

void RenderThreadImpl::ScheduleIdleHandler(int64 initial_delay_ms) {
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
      base::allocator::ReleaseFreeMemory();
      base::DiscardableMemory::ReduceMemoryUsage();
    }
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
    return;
  }

  base::allocator::ReleaseFreeMemory();
  discardable_shared_memory_manager()->ReleaseFreeMemory();

  // Continue the idle timer if the webkit shared timer is not suspended or
  // something is left to do.
  bool continue_timer = !webkit_shared_timer_suspended_;

  if (blink::mainThreadIsolate() &&
      !blink::mainThreadIsolate()->IdleNotification(1000)) {
    continue_timer = true;
  }
  if (!base::DiscardableMemory::ReduceMemoryUsage()) {
    continue_timer = true;
  }

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

int64 RenderThreadImpl::GetIdleNotificationDelayInMs() const {
  return idle_notification_delay_in_ms_;
}

void RenderThreadImpl::SetIdleNotificationDelayInMs(
    int64 idle_notification_delay_in_ms) {
  idle_notification_delay_in_ms_ = idle_notification_delay_in_ms;
}

void RenderThreadImpl::UpdateHistograms(int sequence_number) {
  child_histogram_message_filter()->SendHistograms(sequence_number);
}

int RenderThreadImpl::PostTaskToAllWebWorkers(const base::Closure& closure) {
  return WorkerTaskRunner::Instance()->PostTaskToAllThreads(closure);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
}

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

scoped_refptr<media::GpuVideoAcceleratorFactories>
RenderThreadImpl::GetGpuFactories() {
  DCHECK(IsMainThread());

  scoped_refptr<GpuChannelHost> gpu_channel_host = GetGpuChannel();
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  scoped_refptr<media::GpuVideoAcceleratorFactories> gpu_factories;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner =
      GetMediaThreadTaskRunner();
  if (!cmd_line->HasSwitch(switches::kDisableAcceleratedVideoDecode)) {
    if (!gpu_va_context_provider_.get() ||
        gpu_va_context_provider_->DestroyedOnMainThread()) {
      if (!gpu_channel_host.get()) {
        gpu_channel_host = EstablishGpuChannelSync(
            CAUSE_FOR_GPU_LAUNCH_WEBGRAPHICSCONTEXT3DCOMMANDBUFFERIMPL_INITIALIZE);
      }
      blink::WebGraphicsContext3D::Attributes attributes;
      bool lose_context_when_out_of_memory = false;
      gpu_va_context_provider_ = ContextProviderCommandBuffer::Create(
          make_scoped_ptr(
              WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
                  gpu_channel_host.get(),
                  attributes,
                  lose_context_when_out_of_memory,
                  GURL("chrome://gpu/RenderThreadImpl::GetGpuVDAContext3D"),
                  WebGraphicsContext3DCommandBufferImpl::SharedMemoryLimits(),
                  NULL)),
          "GPU-VideoAccelerator-Offscreen");
    }
  }
  if (gpu_va_context_provider_.get()) {
    gpu_factories = RendererGpuVideoAcceleratorFactories::Create(
        gpu_channel_host.get(), media_task_runner, gpu_va_context_provider_);
  }
  return gpu_factories;
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
      shared_main_thread_contexts_->DestroyedOnMainThread()) {
    shared_main_thread_contexts_ = NULL;
#if defined(OS_ANDROID)
    if (SynchronousCompositorFactory* factory =
            SynchronousCompositorFactory::GetInstance()) {
      shared_main_thread_contexts_ = factory->CreateOffscreenContextProvider(
          GetOffscreenAttribs(), "Offscreen-MainThread");
    }
#endif
    if (!shared_main_thread_contexts_.get()) {
      shared_main_thread_contexts_ = ContextProviderCommandBuffer::Create(
          CreateOffscreenContext3d(), "Offscreen-MainThread");
    }
    if (shared_main_thread_contexts_.get() &&
        !shared_main_thread_contexts_->BindToCurrentThread())
      shared_main_thread_contexts_ = NULL;
  }
  return shared_main_thread_contexts_;
}

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_) {
    audio_renderer_mixer_manager_.reset(new AudioRendererMixerManager(
        GetAudioHardwareConfig()));
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
    audio_message_filter_->SetAudioHardwareConfig(audio_hardware_config_.get());
  }

  return audio_hardware_config_.get();
}

base::WaitableEvent* RenderThreadImpl::GetShutdownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

#if defined(OS_WIN)
void RenderThreadImpl::PreCacheFontCharacters(const LOGFONT& log_font,
                                              const base::string16& str) {
  Send(new ViewHostMsg_PreCacheFontCharacters(log_font, str));
}

#endif  // OS_WIN

ServiceRegistry* RenderThreadImpl::GetServiceRegistry() {
  return service_registry();
}

bool RenderThreadImpl::IsImplSidePaintingEnabled() {
  return is_impl_side_painting_enabled_;
}

bool RenderThreadImpl::IsGpuRasterizationForced() {
  return is_gpu_rasterization_forced_;
}

bool RenderThreadImpl::IsGpuRasterizationEnabled() {
  return is_gpu_rasterization_enabled_;
}

bool RenderThreadImpl::IsThreadedGpuRasterizationEnabled() {
  return is_threaded_gpu_rasterization_enabled_;
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

bool RenderThreadImpl::IsOneCopyEnabled() {
  return is_one_copy_enabled_;
}

bool RenderThreadImpl::IsElasticOverscrollEnabled() {
  return is_elastic_overscroll_enabled_;
}

bool RenderThreadImpl::UseSingleThreadScheduler() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  return !cmd->HasSwitch(switches::kDisableSingleThreadProxyScheduler);
}

uint32 RenderThreadImpl::GetImageTextureTarget() {
  return use_image_texture_target_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorMainThreadTaskRunner() {
  return main_thread_compositor_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
RenderThreadImpl::GetCompositorImplThreadTaskRunner() {
  return compositor_message_loop_proxy_;
}

gpu::GpuMemoryBufferManager* RenderThreadImpl::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager();
}

RendererScheduler* RenderThreadImpl::GetRendererScheduler() {
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
    return factory->CreateExternalBeginFrameSource(routing_id);
  }
#endif
  return make_scoped_ptr(new CompositorExternalBeginFrameSource(
      compositor_message_filter_.get(), sync_message_filter(), routing_id));
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

base::MessageLoop* RenderThreadImpl::GetMainLoop() {
  return message_loop();
}

scoped_refptr<base::MessageLoopProxy> RenderThreadImpl::GetIOLoopProxy() {
  return io_message_loop_proxy_;
}

scoped_ptr<base::SharedMemory> RenderThreadImpl::AllocateSharedMemory(
    size_t size) {
  return scoped_ptr<base::SharedMemory>(
      HostAllocateSharedMemoryBuffer(size));
}

CreateCommandBufferResult RenderThreadImpl::CreateViewCommandBuffer(
      int32 surface_id,
      const GPUCreateCommandBufferConfig& init_params,
      int32 route_id) {
  TRACE_EVENT1("gpu",
               "RenderThreadImpl::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  CreateCommandBufferResult result = CREATE_COMMAND_BUFFER_FAILED;
  IPC::Message* message = new GpuHostMsg_CreateViewCommandBuffer(
      surface_id,
      init_params,
      route_id,
      &result);

  // Allow calling this from the compositor thread.
  thread_safe_sender()->Send(message);

  return result;
}

void RenderThreadImpl::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThreadImpl::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
  ObserverListBase<RenderProcessObserver>::Iterator it(observers_);
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
    IPC_MESSAGE_HANDLER(ViewMsg_NetworkTypeChanged, OnNetworkTypeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_TempCrashWithData, OnTempCrashWithData)
    IPC_MESSAGE_HANDLER(WorkerProcessMsg_CreateWorker, OnCreateNewSharedWorker)
    IPC_MESSAGE_HANDLER(ViewMsg_TimezoneChange, OnUpdateTimezone)
#if defined(OS_ANDROID)
    IPC_MESSAGE_HANDLER(ViewMsg_SetWebKitSharedTimersSuspended,
                        OnSetWebKitSharedTimersSuspended)
#endif
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateScrollbarTheme, OnUpdateScrollbarTheme)
#endif
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache, OnPurgePluginListCache)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnCreateNewFrame(
    int routing_id,
    int parent_routing_id,
    int proxy_routing_id,
    const FrameReplicationState& replicated_state,
    FrameMsg_NewFrame_WidgetParams params) {
  CompositorDependencies* compositor_deps = this;
  RenderFrameImpl::CreateFrame(routing_id, parent_routing_id, proxy_routing_id,
                               replicated_state, compositor_deps, params);
}

void RenderThreadImpl::OnCreateNewFrameProxy(
    int routing_id,
    int parent_routing_id,
    int render_view_routing_id,
    const FrameReplicationState& replicated_state) {
  RenderFrameProxy::CreateFrameProxy(routing_id, parent_routing_id,
                                     render_view_routing_id, replicated_state);
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
  RenderViewImpl::Create(params, compositor_deps, false);
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
  io_message_loop_proxy_ = ChildProcess::current()->io_message_loop_proxy();

  gpu_channel_ =
      GpuChannelHost::Create(this,
                             gpu_info,
                             channel_handle,
                             ChildProcess::current()->GetShutDownEvent(),
                             gpu_memory_buffer_manager());
  return gpu_channel_.get();
}

blink::WebMediaStreamCenter* RenderThreadImpl::CreateMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebRTC))
    return NULL;
#endif

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

void RenderThreadImpl::OnNetworkTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  EnsureWebKitInitialized();
  bool online = type != net::NetworkChangeNotifier::CONNECTION_NONE;
  WebNetworkStateNotifier::setOnLine(online);
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, NetworkStateChanged(online));
  WebNetworkStateNotifier::setWebConnectionType(
      NetConnectionTypeToWebConnectionType(type));
}

void RenderThreadImpl::OnTempCrashWithData(const GURL& data) {
  GetContentClient()->SetActiveURL(data);
  CHECK(false);
}

void RenderThreadImpl::OnUpdateTimezone() {
  if (!blink_platform_impl_)
    return;
  NotifyTimezoneChange();
}

#if defined(OS_ANDROID)
void RenderThreadImpl::OnSetWebKitSharedTimersSuspended(bool suspend) {
  if (suspend_webkit_shared_timer_) {
    EnsureWebKitInitialized();
    if (suspend) {
      blink_platform_impl_->SuspendSharedTimer();
    } else {
      blink_platform_impl_->ResumeSharedTimer();
    }
    webkit_shared_timer_suspended_ = suspend;
  }
}
#endif

#if defined(OS_MACOSX)
void RenderThreadImpl::OnUpdateScrollbarTheme(
    float initial_button_delay,
    float autoscroll_button_delay,
    bool jump_on_track_click,
    blink::ScrollerStyle preferred_scroller_style,
    bool redraw) {
  EnsureWebKitInitialized();
  static_cast<WebScrollbarBehaviorImpl*>(
      blink_platform_impl_->scrollbarBehavior())
      ->set_jump_on_track_click(jump_on_track_click);
  blink::WebScrollbarTheme::updateScrollbars(initial_button_delay,
                                             autoscroll_button_delay,
                                             preferred_scroller_style,
                                             redraw);
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
  base::allocator::ReleaseFreeMemory();

  // Do not call into blink if it is not initialized.
  if (blink_platform_impl_) {
    blink::WebCache::pruneAll();

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
      // Clear the image cache.
      blink::WebImageCache::clear();

      // Purge Skia font cache, by setting it to 0 and then again to the
      // previous limit.
      size_t font_cache_limit = SkGraphics::SetFontCacheLimit(0);
      SkGraphics::SetFontCacheLimit(font_cache_limit);
    }
  }
}

scoped_refptr<base::MessageLoopProxy>
RenderThreadImpl::GetFileThreadMessageLoopProxy() {
  DCHECK(message_loop() == base::MessageLoop::current());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->message_loop_proxy();
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
  return media_thread_->message_loop_proxy();
}

void RenderThreadImpl::SampleGamepads(blink::WebGamepads* data) {
  blink_platform_impl_->sampleGamepads(*data);
}

void RenderThreadImpl::WidgetCreated() {
  widget_count_++;
}

void RenderThreadImpl::WidgetDestroyed() {
  widget_count_--;
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK_LT(hidden_widget_count_, widget_count_);
  hidden_widget_count_++;

  if (widget_count_ && hidden_widget_count_ == widget_count_) {
    // TODO(reveman): Remove this when we have a better mechanism to prevent
    // total discardable memory used by all renderers from growing too large.
    base::internal::DiscardableMemoryEmulated::
        ReduceMemoryUsageUntilWithinLimit(
            kEmulatedDiscardableMemoryBytesToKeepWhenWidgetsHidden);

    if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
      ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
  }
}

void RenderThreadImpl::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

RenderThreadImpl::PendingRenderFrameConnect::PendingRenderFrameConnect(
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services)
    : services(services.Pass()),
      exposed_services(exposed_services.Pass()) {
}

RenderThreadImpl::PendingRenderFrameConnect::~PendingRenderFrameConnect() {
}

}  // namespace content
