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
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"  // Temporary
#include "base/threading/thread_local.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/common/appcache/appcache_dispatcher.h"
#include "content/common/child_histogram_message_filter.h"
#include "content/common/child_process_messages.h"
#include "content/common/database_messages.h"
#include "content/common/db_message_filter.h"
#include "content/common/dom_storage_messages.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/indexed_db/indexed_db_dispatcher.h"
#include "content/common/indexed_db/indexed_db_message_filter.h"
#include "content/common/npobject_util.h"
#include "content/common/plugin_messages.h"
#include "content/common/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/common/web_database_observer_impl.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/devtools/devtools_agent_filter.h"
#include "content/renderer/dom_storage/dom_storage_dispatcher.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/gpu/compositor_thread.h"
#include "content/renderer/gpu/compositor_output_surface.h"
#include "content/renderer/gpu/gpu_benchmarking_extension.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "content/renderer/media/media_stream_center.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "content/renderer/plugin_channel_host.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "grit/content_resources.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_forwarding_message_filter.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColorName.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNetworkStateNotifier.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorkerRepository.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_switches.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#include "base/win/scoped_com_initializer.h"
#else
// TODO(port)
#include "base/memory/scoped_handle.h"
#include "content/common/np_channel_base.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "third_party/webrtc/system_wrappers/interface/event_tracer.h"
#endif

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebNetworkStateNotifier;
using WebKit::WebRuntimeFeatures;
using WebKit::WebScriptController;
using WebKit::WebSecurityPolicy;
using WebKit::WebString;
using WebKit::WebView;

namespace content {

namespace {

const int64 kInitialIdleHandlerDelayMs = 1000;
const int64 kShortIdleHandlerDelayMs = 1000;
const int64 kLongIdleHandlerDelayMs = 30*1000;
const int kIdleCPUUsageThresholdInPercents = 3;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

class RenderViewZoomer : public RenderViewVisitor {
 public:
  RenderViewZoomer(const std::string& host, double zoom_level)
      : host_(host), zoom_level_(zoom_level) {
  }

  virtual bool Visit(RenderView* render_view) {
    WebView* webview = render_view->GetWebView();
    WebDocument document = webview->mainFrame()->document();

    // Don't set zoom level for full-page plugin since they don't use the same
    // zoom settings.
    if (document.isPluginDocument())
      return true;

    if (net::GetHostOrSpecFromURL(GURL(document.url())) == host_)
      webview->setZoomLevel(false, zoom_level_);
    return true;
  }

 private:
  std::string host_;
  double zoom_level_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewZoomer);
};

std::string HostToCustomHistogramSuffix(const std::string& host) {
  if (host == "mail.google.com")
    return ".gmail";
  if (host == "docs.google.com" || host == "drive.google.com")
    return ".docs";
  if (host == "plus.google.com")
    return ".plus";
  return "";
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
  base::Histogram* histogram = base::Histogram::FactoryGet(
      histogram_name, min, max, buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

#if defined(ENABLE_WEBRTC)
const unsigned char* GetCategoryEnabled(const char* name) {
  return TRACE_EVENT_API_GET_CATEGORY_ENABLED(name);
}

void AddTraceEvent(char phase,
                   const unsigned char* category_enabled,
                   const char* name,
                   unsigned long long id,
                   int num_args,
                   const char** arg_names,
                   const unsigned char* arg_types,
                   const unsigned long long* arg_values,
                   unsigned char flags) {
  TRACE_EVENT_API_ADD_TRACE_EVENT(phase, category_enabled, name, id, num_args,
                                  arg_names, arg_types, arg_values, flags);
}
#endif

}  // namespace

class RenderThreadImpl::GpuVDAContextLostCallback
    : public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
 public:
  GpuVDAContextLostCallback() {}
  virtual ~GpuVDAContextLostCallback() {}
  virtual void onContextLost() {
    ChildThread::current()->message_loop()->PostTask(FROM_HERE, base::Bind(
        &RenderThreadImpl::OnGpuVDAContextLoss));
  }
};

RenderThreadImpl::HistogramCustomizer::HistogramCustomizer() {
  custom_histograms_.insert("V8.MemoryExternalFragmentationTotal");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalCommitted");
  custom_histograms_.insert("V8.MemoryHeapSampleTotalUsed");
}

RenderThreadImpl::HistogramCustomizer::~HistogramCustomizer() {}

void RenderThreadImpl::HistogramCustomizer::RenderViewNavigatedToHost(
    const std::string& host, size_t view_count) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
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
    v8::V8::SetCreateHistogramFunction(CreateHistogram);
  }
}

RenderThreadImpl* RenderThreadImpl::current() {
  return lazy_tls.Pointer()->Get();
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThreadImpl::RenderThreadImpl() {
  Init();
}

RenderThreadImpl::RenderThreadImpl(const std::string& channel_name)
    : ChildThread(channel_name) {
  Init();
}

void RenderThreadImpl::Init() {
  TRACE_EVENT_BEGIN_ETW("RenderThreadImpl::Init", 0, "");

  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);
  v8::V8::SetCreateHistogramFunction(CreateHistogram);
  v8::V8::SetAddHistogramSampleFunction(AddHistogramSample);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // On Mac and Android, the select popups are rendered by the browser.
  WebKit::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);

#if defined(OS_WIN)
  // If you are running plugins in this thread you need COM active but in
  // the normal case you don't.
  if (RenderProcessImpl::InProcessPlugins())
    initialize_com_.reset(new base::win::ScopedCOMInitializer());
#endif

  // Register this object as the main thread.
  ChildProcess::current()->set_main_thread(this);

  // In single process the single process is all there is.
  suspend_webkit_shared_timer_ = true;
  notify_webkit_of_modal_loop_ = true;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_ms_ = kInitialIdleHandlerDelayMs;
  idle_notifications_to_skip_ = 0;
  compositor_initialized_ = false;

  appcache_dispatcher_.reset(new AppCacheDispatcher(Get()));
  dom_storage_dispatcher_.reset(new DomStorageDispatcher());
  main_thread_indexed_db_dispatcher_.reset(new IndexedDBDispatcher());

  media_stream_center_ = NULL;

  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

#if defined(ENABLE_WEBRTC)
  webrtc::SetupEventTracer(&GetCategoryEnabled, &AddTraceEvent);

  peer_connection_tracker_.reset(new PeerConnectionTracker());
  AddObserver(peer_connection_tracker_.get());

  p2p_socket_dispatcher_ = new P2PSocketDispatcher(GetIOMessageLoopProxy());
  AddFilter(p2p_socket_dispatcher_);
#endif  // defined(ENABLE_WEBRTC)
  vc_manager_ = new VideoCaptureImplManager();
  AddFilter(vc_manager_->video_capture_message_filter());

  audio_input_message_filter_ = new AudioInputMessageFilter();
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter();
  AddFilter(audio_message_filter_.get());

  AddFilter(new IndexedDBMessageFilter);

  GetContentClient()->renderer()->RenderThreadStarted();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableGpuBenchmarking))
      RegisterExtension(GpuBenchmarkingExtension::Get());

  context_lost_cb_.reset(new GpuVDAContextLostCallback());

  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  FilePath media_path;
  PathService::Get(DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);

  TRACE_EVENT_END_ETW("RenderThreadImpl::Init", 0, "");
}

RenderThreadImpl::~RenderThreadImpl() {
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, OnRenderProcessShutdown());

  // Wait for all databases to be closed.
  if (web_database_observer_impl_.get())
    web_database_observer_impl_->WaitForAllDatabasesToClose();

  // Shutdown in reverse of the initialization order.
  if (devtools_agent_message_filter_.get()) {
    RemoveFilter(devtools_agent_message_filter_.get());
    devtools_agent_message_filter_ = NULL;
  }

  RemoveFilter(audio_input_message_filter_.get());
  audio_input_message_filter_ = NULL;

  RemoveFilter(audio_message_filter_.get());
  audio_message_filter_ = NULL;

  RemoveFilter(vc_manager_->video_capture_message_filter());

  RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;

  // Shutdown the file thread if it's running.
  if (file_thread_.get())
    file_thread_->Stop();

  if (compositor_output_surface_filter_.get()) {
    RemoveFilter(compositor_output_surface_filter_.get());
    compositor_output_surface_filter_ = NULL;
  }

  if (compositor_initialized_) {
    WebKit::Platform::current()->compositorSupport()->shutdown();
    compositor_initialized_ = false;
  }
  if (compositor_thread_.get()) {
    RemoveFilter(compositor_thread_->GetMessageFilter());
    compositor_thread_.reset();
  }

  if (webkit_platform_support_.get())
    WebKit::shutdown();

  lazy_tls.Pointer()->Set(NULL);

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  NPChannelBase::CleanupChannels();
#endif
}

bool RenderThreadImpl::Send(IPC::Message* msg) {
  // Certain synchronous messages cannot always be processed synchronously by
  // the browser, e.g., Chrome frame communicating with the embedding browser.
  // This could cause a complete hang of Chrome if a windowed plug-in is trying
  // to communicate with the renderer thread since the browser's UI thread
  // could be stuck (within a Windows API call) trying to synchronously
  // communicate with the plug-in.  The remedy is to pump messages on this
  // thread while the browser is processing this request. This creates an
  // opportunity for re-entrancy into WebKit, so we need to take care to disable
  // callbacks, timers, and pending network loads that could trigger such
  // callbacks.
  bool pumping_events = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    } else {
      if ((msg->type() == ViewHostMsg_GetCookies::ID ||
           msg->type() == ViewHostMsg_GetRawCookies::ID ||
           msg->type() == ViewHostMsg_CookiesEnabled::ID) &&
          GetContentClient()->renderer()->
              ShouldPumpEventsDuringCookieMessage()) {
        pumping_events = true;
      }
    }
  }

  bool suspend_webkit_shared_timer = true;  // default value
  std::swap(suspend_webkit_shared_timer, suspend_webkit_shared_timer_);

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

  int render_view_id = MSG_ROUTING_NONE;

  if (pumping_events) {
    if (suspend_webkit_shared_timer)
      webkit_platform_support_->SuspendSharedTimer();

    if (notify_webkit_of_modal_loop)
      WebView::willEnterModalLoop();

    RenderWidget* widget =
        static_cast<RenderWidget*>(ResolveRoute(msg->routing_id()));
    if (widget) {
      render_view_id = widget->routing_id();
      PluginChannelHost::Broadcast(
          new PluginMsg_SignalModalDialogEvent(render_view_id));
    }
  }

  bool rv = ChildThread::Send(msg);

  if (pumping_events) {
    if (render_view_id != MSG_ROUTING_NONE) {
      PluginChannelHost::Broadcast(
          new PluginMsg_ResetModalDialogEvent(render_view_id));
    }

    if (notify_webkit_of_modal_loop)
      WebView::didExitModalLoop();

    if (suspend_webkit_shared_timer)
      webkit_platform_support_->ResumeSharedTimer();
  }

  return rv;
}

MessageLoop* RenderThreadImpl::GetMessageLoop() {
  return message_loop();
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
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
  widget_count_++;
  return ChildThread::AddRoute(routing_id, listener);
}

void RenderThreadImpl::RemoveRoute(int32 routing_id) {
  widget_count_--;
  return ChildThread::RemoveRoute(routing_id);
}

int RenderThreadImpl::GenerateRoutingID() {
  int routing_id = MSG_ROUTING_NONE;
  Send(new ViewHostMsg_GenerateRoutingID(&routing_id));
  return routing_id;
}

void RenderThreadImpl::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThreadImpl::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThreadImpl::SetOutgoingMessageFilter(
    IPC::ChannelProxy::OutgoingMessageFilter* filter) {
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

void RenderThreadImpl::WidgetHidden() {
  DCHECK(hidden_widget_count_ < widget_count_);
  hidden_widget_count_++;

  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  if (widget_count_ && hidden_widget_count_ == widget_count_)
    ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
}

void RenderThreadImpl::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (!GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

void RenderThreadImpl::EnsureWebKitInitialized() {
  if (webkit_platform_support_.get())
    return;

  webkit_platform_support_.reset(new RendererWebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());
  WebKit::setSharedWorkerRepository(
      webkit_platform_support_.get()->sharedWorkerRepository());
  WebKit::setIDBFactory(
      webkit_platform_support_.get()->idbFactory());

  WebKit::WebCompositorSupport* compositor_support =
      WebKit::Platform::current()->compositorSupport();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // TODO(fsamuel): Guests don't currently support threaded compositing.
  // This should go away with the new design of the browser plugin.
  // The new design can be tracked at: http://crbug.com/134492.
  bool is_guest = command_line.HasSwitch(switches::kGuestRenderer);
  bool threaded = command_line.HasSwitch(switches::kEnableThreadedCompositing);

  bool enable = threaded && !is_guest;
  if (enable) {
    compositor_thread_.reset(new CompositorThread(this));
    AddFilter(compositor_thread_->GetMessageFilter());
    compositor_support->initialize(compositor_thread_->GetWebThread());
  } else {
    compositor_support->initialize(NULL);
  }
  compositor_initialized_ = true;

  MessageLoop* output_surface_loop = enable ?
      compositor_thread_->message_loop() :
      MessageLoop::current();

  compositor_output_surface_filter_ = CompositorOutputSurface::CreateFilter(
      output_surface_loop->message_loop_proxy());
  AddFilter(compositor_output_surface_filter_.get());

  WebScriptController::enableV8SingleThreadMode();

  RenderThreadImpl::RegisterSchemes();

  webkit_glue::EnableWebCoreLogChannels(
      command_line.GetSwitchValueASCII(switches::kWebCoreLogChannels));

  if (command_line.HasSwitch(switches::kDomAutomationController)) {
    base::StringPiece extension = GetContentClient()->GetDataResource(
        IDR_DOM_AUTOMATION_JS, ui::SCALE_FACTOR_NONE);
    RegisterExtension(new v8::Extension(
        "dom_automation.js", extension.data(), 0, NULL, extension.size()));
  }

  web_database_observer_impl_.reset(
      new WebDatabaseObserverImpl(sync_message_filter()));
  WebKit::WebDatabase::setObserver(web_database_observer_impl_.get());

  WebRuntimeFeatures::enableSockets(
      !command_line.HasSwitch(switches::kDisableWebSockets));

  WebRuntimeFeatures::enableDatabase(
      !command_line.HasSwitch(switches::kDisableDatabases));

  WebRuntimeFeatures::enableDataTransferItems(
      !command_line.HasSwitch(switches::kDisableDataTransferItems));

  WebRuntimeFeatures::enableApplicationCache(
      !command_line.HasSwitch(switches::kDisableApplicationCache));

  WebRuntimeFeatures::enableNotifications(
      !command_line.HasSwitch(switches::kDisableDesktopNotifications));

  WebRuntimeFeatures::enableLocalStorage(
      !command_line.HasSwitch(switches::kDisableLocalStorage));
  WebRuntimeFeatures::enableSessionStorage(
      !command_line.HasSwitch(switches::kDisableSessionStorage));

  WebRuntimeFeatures::enableIndexedDatabase(true);

  WebRuntimeFeatures::enableGeolocation(
      !command_line.HasSwitch(switches::kDisableGeolocation));

  WebKit::WebRuntimeFeatures::enableMediaSource(
      !command_line.HasSwitch(switches::kDisableMediaSource));

  WebRuntimeFeatures::enableMediaPlayer(
      media::IsMediaLibraryInitialized());

  WebKit::WebRuntimeFeatures::enableMediaStream(true);
  WebKit::WebRuntimeFeatures::enablePeerConnection(true);

  WebKit::WebRuntimeFeatures::enableFullScreenAPI(
      !command_line.HasSwitch(switches::kDisableFullScreen));

  WebKit::WebRuntimeFeatures::enableEncryptedMedia(
      command_line.HasSwitch(switches::kEnableEncryptedMedia));

#if defined(OS_ANDROID)
  WebRuntimeFeatures::enableWebAudio(
      command_line.HasSwitch(switches::kEnableWebAudio) &&
      media::IsMediaLibraryInitialized());
#else
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio) &&
      media::IsMediaLibraryInitialized());
#endif

  WebRuntimeFeatures::enableDeviceMotion(
      command_line.HasSwitch(switches::kEnableDeviceMotion));

  WebRuntimeFeatures::enableDeviceOrientation(
      !command_line.HasSwitch(switches::kDisableDeviceOrientation));

  WebRuntimeFeatures::enableSpeechInput(
      !command_line.HasSwitch(switches::kDisableSpeechInput));

  WebRuntimeFeatures::enableScriptedSpeech(true);

  WebRuntimeFeatures::enableFileSystem(
      !command_line.HasSwitch(switches::kDisableFileSystem));

  WebRuntimeFeatures::enableJavaScriptI18NAPI(
      !command_line.HasSwitch(switches::kDisableJavaScriptI18NAPI));

  WebRuntimeFeatures::enableGamepad(true);

  WebRuntimeFeatures::enableQuota(true);

  WebRuntimeFeatures::enableShadowDOM(true);

  if (command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures)) {
    WebRuntimeFeatures::enableStyleScoped(true);
    WebRuntimeFeatures::enableCSSExclusions(true);
    WebRuntimeFeatures::enableExperimentalContentSecurityPolicyFeatures(true);
    WebRuntimeFeatures::enableCSSRegions(true);
    WebRuntimeFeatures::enableDialogElement(true);
  }

  WebRuntimeFeatures::enableWebIntents(
      command_line.HasSwitch(switches::kWebIntentsInvocationEnabled));

  WebRuntimeFeatures::enableSeamlessIFrames(
      command_line.HasSwitch(switches::kEnableExperimentalWebKitFeatures));

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, WebKitInitialized());

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  if (GetContentClient()->renderer()->RunIdleHandlerWhenWidgetsHidden())
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

void RenderThreadImpl::RegisterSchemes() {
  // swappedout: pages should not be accessible, and should also
  // be treated as empty documents that can commit synchronously.
  WebString swappedout_scheme(ASCIIToUTF16(chrome::kSwappedOutScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(swappedout_scheme);
  WebSecurityPolicy::registerURLSchemeAsEmptyDocument(swappedout_scheme);
}

void RenderThreadImpl::RecordUserMetrics(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

scoped_ptr<base::SharedMemory>
    RenderThreadImpl::HostAllocateSharedMemoryBuffer(uint32 size) {
  base::SharedMemoryHandle handle;
  bool success;
  IPC::Message* message =
      new ChildProcessHostMsg_SyncAllocateSharedMemory(size, &handle);

  // Allow calling this from the compositor thread.
  if (MessageLoop::current() == message_loop())
    success = ChildThread::Send(message);
  else
    success = sync_message_filter()->Send(message);

  if (!success)
    return scoped_ptr<base::SharedMemory>();

  if (!base::SharedMemory::IsHandleValid(handle))
    return scoped_ptr<base::SharedMemory>();

  return scoped_ptr<base::SharedMemory>(new base::SharedMemory(handle, false));
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
    IdleHandlerInForegroundTab();
    return;
  }

  base::allocator::ReleaseFreeMemory();

  v8::V8::IdleNotification();

  // Schedule next invocation.
  // Dampen the delay using the algorithm (if delay is in seconds):
  //    delay = delay + 1 / (delay + 2)
  // Using floor(delay) has a dampening effect such as:
  //    1s, 1, 1, 2, 2, 2, 2, 3, 3, ...
  // If the delay is in milliseconds, the above formula is equivalent to:
  //    delay_ms / 1000 = delay_ms / 1000 + 1 / (delay_ms / 1000 + 2)
  // which is equivalent to
  //    delay_ms = delay_ms + 1000*1000 / (delay_ms + 2000).
  // Note that idle_notification_delay_in_ms_ would be reset to
  // kInitialIdleHandlerDelayMs in RenderThreadImpl::WidgetHidden.
  ScheduleIdleHandler(idle_notification_delay_in_ms_ +
                      1000000 / (idle_notification_delay_in_ms_ + 2000));

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, IdleNotification());
}

void RenderThreadImpl::IdleHandlerInForegroundTab() {
  // Increase the delay in the same way as in IdleHandler,
  // but make it periodic by reseting it once it is too big.
  int64 new_delay_ms = idle_notification_delay_in_ms_ +
                       1000000 / (idle_notification_delay_in_ms_ + 2000);
  if (new_delay_ms >= kLongIdleHandlerDelayMs)
    new_delay_ms = kShortIdleHandlerDelayMs;

  if (idle_notifications_to_skip_ > 0) {
    idle_notifications_to_skip_--;
  } else  {
    int cpu_usage = 0;
    Send(new ViewHostMsg_GetCPUUsage(&cpu_usage));
    // Idle notification hint roughly specifies the expected duration of the
    // idle pause. We set it proportional to the idle timer delay.
    int idle_hint = static_cast<int>(new_delay_ms / 10);
    if (cpu_usage < kIdleCPUUsageThresholdInPercents) {
      base::allocator::ReleaseFreeMemory();
      if (v8::V8::IdleNotification(idle_hint)) {
        // V8 finished collecting garbage.
        new_delay_ms = kLongIdleHandlerDelayMs;
      }
    }
  }
  ScheduleIdleHandler(new_delay_ms);
}

int64 RenderThreadImpl::GetIdleNotificationDelayInMs() const {
  return idle_notification_delay_in_ms_;
}

void RenderThreadImpl::SetIdleNotificationDelayInMs(
    int64 idle_notification_delay_in_ms) {
  idle_notification_delay_in_ms_ = idle_notification_delay_in_ms;
}

void RenderThreadImpl::ToggleWebKitSharedTimer(bool suspend) {
  if (suspend_webkit_shared_timer_) {
    EnsureWebKitInitialized();
    if (suspend) {
      webkit_platform_support_->SuspendSharedTimer();
    } else {
      webkit_platform_support_->ResumeSharedTimer();
    }
  }
}

void RenderThreadImpl::UpdateHistograms(int sequence_number) {
  child_histogram_message_filter()->SendHistograms(sequence_number);
}

bool RenderThreadImpl::ResolveProxy(const GURL& url, std::string* proxy_list) {
  bool result = false;
  Send(new ViewHostMsg_ResolveProxy(url, &result, proxy_list));
  return result;
}

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

/* static */
void RenderThreadImpl::OnGpuVDAContextLoss() {
  RenderThreadImpl* self = RenderThreadImpl::current();
  DCHECK(self);
  if (!self->gpu_vda_context3d_.get())
    return;
  if (self->compositor_thread()) {
    self->compositor_thread()->GetWebThread()->message_loop()->DeleteSoon(
        FROM_HERE, self->gpu_vda_context3d_.release());
  } else {
    self->gpu_vda_context3d_.reset();
  }
}

WebGraphicsContext3DCommandBufferImpl*
RenderThreadImpl::GetGpuVDAContext3D() {
  if (!gpu_vda_context3d_.get()) {
    gpu_vda_context3d_.reset(
        WebGraphicsContext3DCommandBufferImpl::CreateOffscreenContext(
            this, WebKit::WebGraphicsContext3D::Attributes(),
            GURL("chrome://gpu/RenderThreadImpl::GetGpuVDAContext3D")));
    if (gpu_vda_context3d_.get())
      gpu_vda_context3d_->setContextLostCallback(context_lost_cb_.get());
  }
  return gpu_vda_context3d_.get();
}

AudioRendererMixerManager* RenderThreadImpl::GetAudioRendererMixerManager() {
  if (!audio_renderer_mixer_manager_.get()) {
    audio_renderer_mixer_manager_.reset(new AudioRendererMixerManager(
        GetAudioOutputSampleRate(),
        GetAudioOutputBufferSize()));
  }

  return audio_renderer_mixer_manager_.get();
}

#if defined(OS_WIN)
void RenderThreadImpl::PreCacheFontCharacters(const LOGFONT& log_font,
                                              const string16& str) {
  Send(new ViewHostMsg_PreCacheFontCharacters(log_font, str));
}

void RenderThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void RenderThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN

bool RenderThreadImpl::IsWebFrameValid(WebKit::WebFrame* web_frame) {
  if (!web_frame)
    return false;  // We must be shutting down.

  RenderViewImpl* render_view = RenderViewImpl::FromWebView(web_frame->view());
  if (!render_view)
    return false;  // We must be shutting down.

  return true;
}

bool RenderThreadImpl::IsMainThread() {
  return !!current();
}

bool RenderThreadImpl::IsIOThread() {
  return MessageLoop::current() == ChildProcess::current()->io_message_loop();
}

MessageLoop* RenderThreadImpl::GetMainLoop() {
  return message_loop();
}

scoped_refptr<base::MessageLoopProxy> RenderThreadImpl::GetIOLoopProxy() {
  return ChildProcess::current()->io_message_loop_proxy();
}

base::WaitableEvent* RenderThreadImpl::GetShutDownEvent() {
  return ChildProcess::current()->GetShutDownEvent();
}

scoped_ptr<base::SharedMemory> RenderThreadImpl::AllocateSharedMemory(
    uint32 size) {
  return scoped_ptr<base::SharedMemory>(
      HostAllocateSharedMemoryBuffer(size));
}

int32 RenderThreadImpl::CreateViewCommandBuffer(
      int32 surface_id, const GPUCreateCommandBufferConfig& init_params) {
  TRACE_EVENT1("gpu",
               "RenderThreadImpl::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  int32 route_id = MSG_ROUTING_NONE;
  IPC::Message* message = new GpuHostMsg_CreateViewCommandBuffer(
      surface_id,
      init_params,
      &route_id);

  // Allow calling this from the compositor thread.
  if (MessageLoop::current() == message_loop())
    ChildThread::Send(message);
  else
    sync_message_filter()->Send(message);

  return route_id;
}

void RenderThreadImpl::CreateImage(
    gfx::PluginWindowHandle window,
    int32 image_id,
    const CreateImageCallback& callback) {
  NOTREACHED();
}

void RenderThreadImpl::DeleteImage(int32 image_id, int32 sync_point) {
  NOTREACHED();
}

void RenderThreadImpl::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThreadImpl::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

void RenderThreadImpl::OnSetZoomLevelForCurrentURL(const std::string& host,
                                                   double zoom_level) {
  RenderViewZoomer zoomer(host, zoom_level);
  RenderView::ForEach(&zoomer);
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
      dom_storage_dispatcher_->OnMessageReceived(msg)) {
    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderThreadImpl, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForCurrentURL,
                        OnSetZoomLevelForCurrentURL)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache, OnPurgePluginListCache)
    IPC_MESSAGE_HANDLER(ViewMsg_NetworkStateChanged, OnNetworkStateChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_TempCrashWithData, OnTempCrashWithData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnCreateNewView(const ViewMsg_New_Params& params) {
  EnsureWebKitInitialized();
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(
      params.opener_route_id,
      params.renderer_preferences,
      params.web_preferences,
      new SharedRenderViewCounter(0),
      params.view_id,
      params.surface_id,
      params.session_storage_namespace_id,
      params.frame_name,
      false,
      params.swapped_out,
      params.next_page_id,
      params.screen_info,
      params.accessibility_mode);
}

GpuChannelHost* RenderThreadImpl::EstablishGpuChannelSync(
    CauseForGpuLaunch cause_for_gpu_launch) {
  TRACE_EVENT0("gpu", "RenderThreadImpl::EstablishGpuChannelSync");

  if (gpu_channel_.get()) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (gpu_channel_->state() == GpuChannelHost::kUnconnected ||
        gpu_channel_->state() == GpuChannelHost::kConnected)
      return GetGpuChannel();

    // Recreate the channel if it has been lost.
    gpu_channel_ = NULL;
  }

  // Ask the browser for the channel name.
  int client_id = 0;
  IPC::ChannelHandle channel_handle;
  GPUInfo gpu_info;
  if (!Send(new GpuHostMsg_EstablishGpuChannel(cause_for_gpu_launch,
                                               &client_id,
                                               &channel_handle,
                                               &gpu_info)) ||
#if defined(OS_POSIX)
      channel_handle.socket.fd == -1 ||
#endif
      channel_handle.name.empty()) {
    // Otherwise cancel the connection.
    gpu_channel_ = NULL;
    return NULL;
  }

  gpu_channel_ = new GpuChannelHost(this, 0, client_id);
  gpu_channel_->set_gpu_info(gpu_info);
  GetContentClient()->SetGpuInfo(gpu_info);

  // Connect to the GPU process if a channel name was received.
  gpu_channel_->Connect(channel_handle);

  return GetGpuChannel();
}

WebKit::WebMediaStreamCenter* RenderThreadImpl::CreateMediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client) {
#if defined(ENABLE_WEBRTC)
  if (!media_stream_center_)
    media_stream_center_ = new MediaStreamCenter(
        client, GetMediaStreamDependencyFactory());
#endif
  return media_stream_center_;
}

MediaStreamDependencyFactory*
RenderThreadImpl::GetMediaStreamDependencyFactory() {
#if defined(ENABLE_WEBRTC)
  if (!media_stream_factory_.get()) {
    media_stream_factory_.reset(new MediaStreamDependencyFactory(
        vc_manager_, p2p_socket_dispatcher_));
  }
#endif
  return media_stream_factory_.get();
}

GpuChannelHost* RenderThreadImpl::GetGpuChannel() {
  if (!gpu_channel_.get())
    return NULL;

  if (gpu_channel_->state() != GpuChannelHost::kConnected)
    return NULL;

  return gpu_channel_.get();
}

void RenderThreadImpl::OnPurgePluginListCache(bool reload_pages) {
  EnsureWebKitInitialized();
  // The call below will cause a GetPlugins call with refresh=true, but at this
  // point we already know that the browser has refreshed its list, so disable
  // refresh temporarily to prevent each renderer process causing the list to be
  // regenerated.
  webkit_platform_support_->set_plugin_refresh_allowed(false);
  WebKit::resetPluginCache(reload_pages);
  webkit_platform_support_->set_plugin_refresh_allowed(true);

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, PluginListChanged());
}

void RenderThreadImpl::OnNetworkStateChanged(bool online) {
  EnsureWebKitInitialized();
  WebNetworkStateNotifier::setOnLine(online);
}

void RenderThreadImpl::OnTempCrashWithData(const GURL& data) {
  GetContentClient()->SetActiveURL(data);
  CHECK(false);
}

scoped_refptr<base::MessageLoopProxy>
RenderThreadImpl::GetFileThreadMessageLoopProxy() {
  DCHECK(message_loop() == MessageLoop::current());
  if (!file_thread_.get()) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->message_loop_proxy();
}

}  // namespace content
