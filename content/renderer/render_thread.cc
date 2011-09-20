// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_table.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "content/common/appcache/appcache_dispatcher.h"
#include "content/common/content_switches.h"
#include "content/common/database_messages.h"
#include "content/common/db_message_filter.h"
#include "content/common/dom_storage_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/plugin_messages.h"
#include "content/common/renderer_preferences.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/common/web_database_observer_impl.h"
#include "content/plugin/npobject_util.h"
#include "content/renderer/content_renderer_client.h"
#include "content/renderer/devtools_agent_filter.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "content/renderer/plugin_channel_host.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_process_observer.h"
#include "content/renderer/render_view.h"
#include "content/renderer/render_view_visitor.h"
#include "content/renderer/renderer_webidbfactory_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNetworkStateNotifier.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"
#include "webkit/extensions/v8/playback_extension.h"
#include "webkit/glue/webkit_glue.h"

// TODO(port)
#if defined(OS_WIN)
#include "content/plugin/plugin_channel.h"
#else
#include "base/memory/scoped_handle.h"
#include "content/common/np_channel_base.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebNetworkStateNotifier;
using WebKit::WebRuntimeFeatures;
using WebKit::WebScriptController;
using WebKit::WebString;
using WebKit::WebStorageEventDispatcher;
using WebKit::WebView;

namespace {
static const double kInitialIdleHandlerDelayS = 1.0 /* seconds */;

#if defined(TOUCH_UI)
static const int kPopupListBoxMinimumRowHeight = 60;
#endif

// Keep the global RenderThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
static base::LazyInstance<base::ThreadLocalPointer<RenderThread> > lazy_tls(
    base::LINKER_INITIALIZED);

class RenderViewZoomer : public RenderViewVisitor {
 public:
  RenderViewZoomer(const GURL& url, double zoom_level)
      : zoom_level_(zoom_level) {
    host_ = net::GetHostOrSpecFromURL(url);
  }

  virtual bool Visit(RenderView* render_view) {
    WebView* webview = render_view->webview();
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

}  // namespace

static void* CreateHistogram(
    const char *name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  base::Histogram* histogram = base::Histogram::FactoryGet(
      name, min, max, buckets, base::Histogram::kUmaTargetedHistogramFlag);
  return histogram;
}

static void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThread::RenderThread() {
  Init();
}

RenderThread::RenderThread(const std::string& channel_name)
    : ChildThread(channel_name) {
  Init();
}

void RenderThread::Init() {
  TRACE_EVENT_BEGIN_ETW("RenderThread::Init", 0, "");

#if defined(OS_MACOSX)
  // On Mac, the select popups are rendered by the browser.
  WebKit::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);
#if defined(OS_WIN)
  // If you are running plugins in this thread you need COM active but in
  // the normal case you don't.
  if (RenderProcessImpl::InProcessPlugins())
    CoInitialize(0);
#endif

  // In single process the single process is all there is.
  suspend_webkit_shared_timer_ = true;
  notify_webkit_of_modal_loop_ = true;
  plugin_refresh_allowed_ = true;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_s_ = kInitialIdleHandlerDelayS;
  task_factory_.reset(new ScopedRunnableMethodFactory<RenderThread>(this));

  appcache_dispatcher_.reset(new AppCacheDispatcher(this));
  indexed_db_dispatcher_.reset(new IndexedDBDispatcher());

  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

  vc_manager_ = new VideoCaptureImplManager();
  AddFilter(vc_manager_->video_capture_message_filter());

  audio_input_message_filter_ = new AudioInputMessageFilter();
  AddFilter(audio_input_message_filter_.get());

  audio_message_filter_ = new AudioMessageFilter();
  AddFilter(audio_message_filter_.get());

  devtools_agent_message_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_message_filter_.get());

  content::GetContentClient()->renderer()->RenderThreadStarted();

  TRACE_EVENT_END_ETW("RenderThread::Init", 0, "");
}

RenderThread::~RenderThread() {
  FOR_EACH_OBSERVER(
      RenderProcessObserver, observers_, OnRenderProcessShutdown());

  // Wait for all databases to be closed.
  if (web_database_observer_impl_.get())
    web_database_observer_impl_->WaitForAllDatabasesToClose();

  // Shutdown in reverse of the initialization order.
  RemoveFilter(devtools_agent_message_filter_.get());
  devtools_agent_message_filter_ = NULL;

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

  if (webkit_platform_support_.get())
    WebKit::shutdown();

  lazy_tls.Pointer()->Set(NULL);

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  NPChannelBase::CleanupChannels();
  // Don't call COM if the renderer is in the sandbox.
  if (RenderProcessImpl::InProcessPlugins())
    CoUninitialize();
#endif
}

RenderThread* RenderThread::current() {
  return lazy_tls.Pointer()->Get();
}

int32 RenderThread::RoutingIDForCurrentContext() {
  int32 routing_id = MSG_ROUTING_CONTROL;
  if (v8::Context::InContext()) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (frame) {
      RenderView* view = RenderView::FromWebView(frame->view());
      if (view)
        routing_id = view->routing_id();
    }
  } else {
    DLOG(WARNING) << "Not called within a script context!";
  }
  return routing_id;
}

bool RenderThread::Send(IPC::Message* msg) {
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
          content::GetContentClient()->renderer()->
              ShouldPumpEventsDuringCookieMessage()) {
        pumping_events = true;
      }
    }
  }

  bool suspend_webkit_shared_timer = true;  // default value
  std::swap(suspend_webkit_shared_timer, suspend_webkit_shared_timer_);

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

  gfx::NativeViewId host_window = 0;

  if (pumping_events) {
    if (suspend_webkit_shared_timer)
      webkit_platform_support_->SuspendSharedTimer();

    if (notify_webkit_of_modal_loop)
      WebView::willEnterModalLoop();

    RenderWidget* widget =
        static_cast<RenderWidget*>(ResolveRoute(msg->routing_id()));
    if (widget) {
      host_window = widget->host_window();
      PluginChannelHost::Broadcast(
          new PluginMsg_SignalModalDialogEvent(host_window));
    }
  }

  bool rv = ChildThread::Send(msg);

  if (pumping_events) {
    if (host_window) {
      PluginChannelHost::Broadcast(
          new PluginMsg_ResetModalDialogEvent(host_window));
    }

    if (notify_webkit_of_modal_loop)
      WebView::didExitModalLoop();

    if (suspend_webkit_shared_timer)
      webkit_platform_support_->ResumeSharedTimer();
  }

  return rv;
}

void RenderThread::AddRoute(int32 routing_id,
                            IPC::Channel::Listener* listener) {
  widget_count_++;
  return ChildThread::AddRoute(routing_id, listener);
}

void RenderThread::RemoveRoute(int32 routing_id) {
  widget_count_--;
  return ChildThread::RemoveRoute(routing_id);
}

void RenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThread::WidgetHidden() {
  DCHECK(hidden_widget_count_ < widget_count_);
  hidden_widget_count_++;

  if (!content::GetContentClient()->renderer()->
          RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  if (widget_count_ && hidden_widget_count_ == widget_count_)
    ScheduleIdleHandler(kInitialIdleHandlerDelayS);
}

void RenderThread::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (!content::GetContentClient()->renderer()->
          RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  idle_timer_.Stop();
}

void RenderThread::AddObserver(RenderProcessObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderThread::RemoveObserver(RenderProcessObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderThread::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThread::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

void RenderThread::OnSetZoomLevelForCurrentURL(const GURL& url,
                                               double zoom_level) {
  RenderViewZoomer zoomer(url, zoom_level);
  RenderView::ForEach(&zoomer);
}

void RenderThread::OnDOMStorageEvent(
    const DOMStorageMsg_Event_Params& params) {
  if (!dom_storage_event_dispatcher_.get())
    dom_storage_event_dispatcher_.reset(WebStorageEventDispatcher::create());
  dom_storage_event_dispatcher_->dispatchStorageEvent(params.key,
      params.old_value, params.new_value, params.origin, params.url,
      params.storage_type == DOM_STORAGE_LOCAL);
}

void RenderThread::EnsureWebKitInitialized() {
  if (webkit_platform_support_.get())
    return;

  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);
  v8::V8::SetCreateHistogramFunction(CreateHistogram);
  v8::V8::SetAddHistogramSampleFunction(AddHistogramSample);

  webkit_platform_support_.reset(new RendererWebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());

  WebScriptController::enableV8SingleThreadMode();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  webkit_glue::EnableWebCoreLogChannels(
      command_line.GetSwitchValueASCII(switches::kWebCoreLogChannels));

  if (command_line.HasSwitch(switches::kPlaybackMode) ||
      command_line.HasSwitch(switches::kRecordMode) ||
      command_line.HasSwitch(switches::kNoJsRandomness)) {
    RegisterExtension(extensions_v8::PlaybackExtension::Get());
  }

  web_database_observer_impl_.reset(new WebDatabaseObserverImpl(this));
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

  WebRuntimeFeatures::enableIndexedDatabase(
      !command_line.HasSwitch(switches::kDisableIndexedDatabase));

  WebRuntimeFeatures::enableGeolocation(
      !command_line.HasSwitch(switches::kDisableGeolocation));

  WebKit::WebRuntimeFeatures::enableMediaStream(
      command_line.HasSwitch(switches::kEnableMediaStream));

  WebKit::WebRuntimeFeatures::enableFullScreenAPI(
      !command_line.HasSwitch(switches::kDisableFullScreen));

#if defined(OS_CHROMEOS)
  // TODO(crogers): enable once Web Audio has been tested and optimized.
  WebRuntimeFeatures::enableWebAudio(false);
#else
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio));
#endif

  WebRuntimeFeatures::enablePushState(true);

#ifdef TOUCH_UI
  WebRuntimeFeatures::enableTouch(true);
  WebKit::WebPopupMenu::setMinimumRowHeight(kPopupListBoxMinimumRowHeight);
#else
  // TODO(saintlou): in the future touch should always be enabled
  WebRuntimeFeatures::enableTouch(false);
#endif

  WebRuntimeFeatures::enableDeviceMotion(
      command_line.HasSwitch(switches::kEnableDeviceMotion));

  WebRuntimeFeatures::enableDeviceOrientation(
      !command_line.HasSwitch(switches::kDisableDeviceOrientation));

  WebRuntimeFeatures::enableSpeechInput(
      !command_line.HasSwitch(switches::kDisableSpeechInput));

  WebRuntimeFeatures::enableFileSystem(
      !command_line.HasSwitch(switches::kDisableFileSystem));

  WebRuntimeFeatures::enableJavaScriptI18NAPI(
      !command_line.HasSwitch(switches::kDisableJavaScriptI18NAPI));

  WebRuntimeFeatures::enableQuota(true);

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, WebKitInitialized());
}

// static
void RenderThread::RecordUserMetrics(const std::string& action) {
  RenderThread::current()->Send(
      new ViewHostMsg_UserMetricsRecordAction(action));
}

#if defined(OS_WIN)
// static
bool RenderThread::PreCacheFont(const LOGFONT& log_font) {
  return RenderThread::current()->Send(new ViewHostMsg_PreCacheFont(log_font));
}
#endif  // OS_WIN

bool RenderThread::OnControlMessageReceived(const IPC::Message& msg) {
  ObserverListBase<RenderProcessObserver>::Iterator it(observers_);
  RenderProcessObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    if (observer->OnControlMessageReceived(msg))
      return true;
  }

  // Some messages are handled by delegates.
  if (appcache_dispatcher_->OnMessageReceived(msg))
    return true;
  if (indexed_db_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderThread, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForCurrentURL,
                        OnSetZoomLevelForCurrentURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetNextPageID, OnSetNextPageID)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCSSColors, OnSetCSSColors)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache, OnPurgePluginListCache)
    IPC_MESSAGE_HANDLER(ViewMsg_NetworkStateChanged, OnNetworkStateChanged)
    IPC_MESSAGE_HANDLER(DOMStorageMsg_Event, OnDOMStorageEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThread::OnSetNextPageID(int32 next_page_id) {
  // This should only be called at process initialization time, so we shouldn't
  // have to worry about thread-safety.
  RenderView::SetNextPageID(next_page_id);
}

// Called when to register CSS Color name->system color mappings.
// We update the colors one by one and then tell WebKit to refresh all render
// views.
void RenderThread::OnSetCSSColors(
    const std::vector<CSSColors::CSSColorMapping>& colors) {
  EnsureWebKitInitialized();
  size_t num_colors = colors.size();
  scoped_array<WebKit::WebColorName> color_names(
      new WebKit::WebColorName[num_colors]);
  scoped_array<WebKit::WebColor> web_colors(new WebKit::WebColor[num_colors]);
  size_t i = 0;
  for (std::vector<CSSColors::CSSColorMapping>::const_iterator it =
          colors.begin();
       it != colors.end();
       ++it, ++i) {
    color_names[i] = it->first;
    web_colors[i] = it->second;
  }
  WebKit::setNamedColors(color_names.get(), web_colors.get(), num_colors);
}

void RenderThread::OnCreateNewView(const ViewMsg_New_Params& params) {
  EnsureWebKitInitialized();
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderView::Create(
      this,
      params.parent_window,
      MSG_ROUTING_NONE,
      params.renderer_preferences,
      params.web_preferences,
      new SharedRenderViewCounter(0),
      params.view_id,
      params.session_storage_namespace_id,
      params.frame_name);
}

GpuChannelHost* RenderThread::EstablishGpuChannelSync(
    content::CauseForGpuLaunch cause_for_gpu_launch) {
  if (gpu_channel_.get()) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (gpu_channel_->state() == GpuChannelHost::kUnconnected ||
        gpu_channel_->state() == GpuChannelHost::kConnected)
      return GetGpuChannel();

    // Recreate the channel if it has been lost.
    if (gpu_channel_->state() == GpuChannelHost::kLost)
      gpu_channel_ = NULL;
  }

  if (!gpu_channel_.get())
    gpu_channel_ = new GpuChannelHost;

  // Ask the browser for the channel name.
  IPC::ChannelHandle channel_handle;
  base::ProcessHandle renderer_process_for_gpu;
  GPUInfo gpu_info;
  if (!Send(new GpuHostMsg_EstablishGpuChannel(cause_for_gpu_launch,
                                               &channel_handle,
                                               &renderer_process_for_gpu,
                                               &gpu_info)) ||
      channel_handle.name.empty() ||
      renderer_process_for_gpu == base::kNullProcessHandle) {
    // Otherwise cancel the connection.
    gpu_channel_ = NULL;
    return NULL;
  }

  gpu_channel_->set_gpu_info(gpu_info);
  content::GetContentClient()->SetGpuInfo(gpu_info);

  // Connect to the GPU process if a channel name was received.
  gpu_channel_->Connect(channel_handle, renderer_process_for_gpu);

  return GetGpuChannel();
}

GpuChannelHost* RenderThread::GetGpuChannel() {
  if (!gpu_channel_.get())
    return NULL;

  if (gpu_channel_->state() != GpuChannelHost::kConnected)
    return NULL;

  return gpu_channel_.get();
}

void RenderThread::IdleHandler() {
#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
  MallocExtension::instance()->ReleaseFreeMemory();
#endif

  v8::V8::IdleNotification();

  // Schedule next invocation.
  // Dampen the delay using the algorithm:
  //    delay = delay + 1 / (delay + 2)
  // Using floor(delay) has a dampening effect such as:
  //    1s, 1, 1, 2, 2, 2, 2, 3, 3, ...
  // Note that idle_notification_delay_in_s_ would be reset to
  // kInitialIdleHandlerDelayS in RenderThread::WidgetHidden.
  ScheduleIdleHandler(idle_notification_delay_in_s_ +
                      1.0 / (idle_notification_delay_in_s_ + 2.0));

  FOR_EACH_OBSERVER(RenderProcessObserver, observers_, IdleNotification());
}

void RenderThread::ScheduleIdleHandler(double initial_delay_s) {
  idle_notification_delay_in_s_ = initial_delay_s;
  idle_timer_.Stop();
  idle_timer_.Start(FROM_HERE,
      base::TimeDelta::FromSeconds(static_cast<int64>(initial_delay_s)),
      this, &RenderThread::IdleHandler);
}

void RenderThread::OnPurgePluginListCache(bool reload_pages) {
  EnsureWebKitInitialized();
  // The call below will cause a GetPlugins call with refresh=true, but at this
  // point we already know that the browser has refreshed its list, so disable
  // refresh temporarily to prevent each renderer process causing the list to be
  // regenerated.
  plugin_refresh_allowed_ = false;
  WebKit::resetPluginCache(reload_pages);
  plugin_refresh_allowed_ = true;
}

void RenderThread::OnNetworkStateChanged(bool online) {
  EnsureWebKitInitialized();
  WebNetworkStateNotifier::setOnLine(online);
}

scoped_refptr<base::MessageLoopProxy>
RenderThread::GetFileThreadMessageLoopProxy() {
  DCHECK(message_loop() == MessageLoop::current());
  if (!file_thread_.get()) {
    file_thread_.reset(new base::Thread("Renderer::FILE"));
    file_thread_->Start();
  }
  return file_thread_->message_loop_proxy();
}

void RenderThread::RegisterExtension(v8::Extension* extension) {
  WebScriptController::registerExtension(extension);
  v8_extensions_.insert(extension->name());
}

bool RenderThread::IsRegisteredExtension(
    const std::string& v8_extension_name) const {
  return v8_extensions_.find(v8_extension_name) != v8_extensions_.end();
}
