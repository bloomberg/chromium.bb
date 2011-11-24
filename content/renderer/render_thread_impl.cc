// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_thread_impl.h"

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
#include "base/win/scoped_com_initializer.h"
#include "content/common/appcache/appcache_dispatcher.h"
#include "content/common/child_process_messages.h"
#include "content/common/database_messages.h"
#include "content/common/db_message_filter.h"
#include "content/common/dom_storage_messages.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/npobject_util.h"
#include "content/common/plugin_messages.h"
#include "content/common/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "content/common/view_messages.h"
#include "content/common/web_database_observer_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/renderer/devtools_agent_filter.h"
#include "content/renderer/gpu/compositor_thread.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/indexed_db_dispatcher.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "content/renderer/plugin_channel_host.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webidbfactory_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
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
#include "ui/base/ui_base_switches.h"
#include "v8/include/v8.h"
#include "webkit/extensions/v8/playback_extension.h"
#include "webkit/glue/webkit_glue.h"

// TODO(port)
#if !defined(OS_WIN)
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
using content::RenderProcessObserver;

namespace {
static const int64 kInitialIdleHandlerDelayMs = 1000;
static const int64 kShortIdleHandlerDelayMs = 1000;
static const int64 kLongIdleHandlerDelayMs = 30*1000;
static const int kIdleCPUUsageThresholdInPercents = 3;

// Keep the global RenderThreadImpl in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
static base::LazyInstance<base::ThreadLocalPointer<RenderThreadImpl> >
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

class RenderViewZoomer : public content::RenderViewVisitor {
 public:
  RenderViewZoomer(const GURL& url, double zoom_level)
      : zoom_level_(zoom_level) {
    host_ = net::GetHostOrSpecFromURL(url);
  }

  virtual bool Visit(content::RenderView* render_view) {
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

#if defined(OS_MACOSX)
  // On Mac, the select popups are rendered by the browser.
  WebKit::WebView::setUseExternalPopupMenus(true);
#endif

  lazy_tls.Pointer()->Set(this);
#if defined(OS_WIN)
  // If you are running plugins in this thread you need COM active but in
  // the normal case you don't.
  if (RenderProcessImpl::InProcessPlugins())
    initialize_com_.reset(new base::win::ScopedCOMInitializer());
#endif

  // In single process the single process is all there is.
  suspend_webkit_shared_timer_ = true;
  notify_webkit_of_modal_loop_ = true;
  plugin_refresh_allowed_ = true;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_ms_ = kInitialIdleHandlerDelayMs;
  idle_notifications_to_skip_ = 0;
  base::ProcessHandle handle = base::GetCurrentProcessHandle();
#if defined(OS_MACOSX)
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle,
                                                                    NULL));
#else
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
#endif
  process_metrics_->GetCPUUsage(); // Initialize CPU usage counters.
  task_factory_.reset(new ScopedRunnableMethodFactory<RenderThreadImpl>(this));

  appcache_dispatcher_.reset(new AppCacheDispatcher(Get()));
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

  TRACE_EVENT_END_ETW("RenderThreadImpl::Init", 0, "");
}

RenderThreadImpl::~RenderThreadImpl() {
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

#ifdef WEBCOMPOSITOR_HAS_INITIALIZE
  WebKit::WebCompositor::shutdown();
#endif
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

MessageLoop* RenderThreadImpl::GetMessageLoop() {
  return message_loop();
}

IPC::SyncChannel* RenderThreadImpl::GetChannel() {
  return channel();
}

std::string RenderThreadImpl::GetLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.  In single process mode, this will return the
  // wrong value.  TODO(tc): Fix this for single process mode.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::string& lang =
      parsed_command_line.GetSwitchValueASCII(switches::kLang);
  DCHECK(!lang.empty() ||
      (!parsed_command_line.HasSwitch(switches::kRendererProcess) &&
       !parsed_command_line.HasSwitch(switches::kPluginProcess)));
  return lang;
}

void RenderThreadImpl::AddRoute(int32 routing_id,
                                IPC::Channel::Listener* listener) {
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

void RenderThreadImpl::AddObserver(content::RenderProcessObserver* observer) {
  observers_.AddObserver(observer);
}

void RenderThreadImpl::RemoveObserver(
    content::RenderProcessObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RenderThreadImpl::SetResourceDispatcherDelegate(
    content::ResourceDispatcherDelegate* delegate) {
  resource_dispatcher()->set_delegate(delegate);
}

void RenderThreadImpl::WidgetHidden() {
  DCHECK(hidden_widget_count_ < widget_count_);
  hidden_widget_count_++;

  if (!content::GetContentClient()->renderer()->
          RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  if (widget_count_ && hidden_widget_count_ == widget_count_)
    ScheduleIdleHandler(kInitialIdleHandlerDelayMs);
}

void RenderThreadImpl::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (!content::GetContentClient()->renderer()->
          RunIdleHandlerWhenWidgetsHidden()) {
    return;
  }

  ScheduleIdleHandler(kLongIdleHandlerDelayMs);
}

void RenderThreadImpl::EnsureWebKitInitialized() {
  if (webkit_platform_support_.get())
    return;

  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);
  v8::V8::SetCreateHistogramFunction(CreateHistogram);
  v8::V8::SetAddHistogramSampleFunction(AddHistogramSample);

  webkit_platform_support_.reset(new RendererWebKitPlatformSupportImpl);
  WebKit::initialize(webkit_platform_support_.get());

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableThreadedCompositing)) {
    compositor_thread_.reset(new CompositorThread(this));
    AddFilter(compositor_thread_->GetMessageFilter());
#ifdef WEBCOMPOSITOR_HAS_INITIALIZE
    WebKit::WebCompositor::initialize(compositor_thread_->GetWebThread());
#else
    WebKit::WebCompositor::setThread(compositor_thread_->GetWebThread());
#endif
  } else {
#ifdef WEBCOMPOSITOR_HAS_INITIALIZE
    WebKit::WebCompositor::initialize(NULL);
#endif
  }

  WebScriptController::enableV8SingleThreadMode();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  webkit_glue::EnableWebCoreLogChannels(
      command_line.GetSwitchValueASCII(switches::kWebCoreLogChannels));

  if (command_line.HasSwitch(switches::kPlaybackMode) ||
      command_line.HasSwitch(switches::kRecordMode) ||
      command_line.HasSwitch(switches::kNoJsRandomness)) {
    RegisterExtension(extensions_v8::PlaybackExtension::Get());
  }

  web_database_observer_impl_.reset(new WebDatabaseObserverImpl(Get()));
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

  WebKit::WebRuntimeFeatures::enableMediaSource(
      command_line.HasSwitch(switches::kEnableMediaSource));

  WebKit::WebRuntimeFeatures::enableMediaStream(
      command_line.HasSwitch(switches::kEnableMediaStream));

  WebKit::WebRuntimeFeatures::enableFullScreenAPI(
      !command_line.HasSwitch(switches::kDisableFullScreen));

  WebKit::WebRuntimeFeatures::enablePointerLock(
      command_line.HasSwitch(switches::kEnablePointerLock));

  WebKit::WebRuntimeFeatures::enableVideoTrack(
      command_line.HasSwitch(switches::kEnableVideoTrack));

#if defined(OS_CHROMEOS)
  // TODO(crogers): enable once Web Audio has been tested and optimized.
  WebRuntimeFeatures::enableWebAudio(false);
#else
  WebRuntimeFeatures::enableWebAudio(
      !command_line.HasSwitch(switches::kDisableWebAudio));
#endif

  WebRuntimeFeatures::enablePushState(true);

  WebRuntimeFeatures::enableTouch(false);

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

  if (content::GetContentClient()->renderer()->
         RunIdleHandlerWhenWidgetsHidden()) {
    ScheduleIdleHandler(kLongIdleHandlerDelayMs);
  }
}

void RenderThreadImpl::RecordUserMetrics(const std::string& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(action));
}

base::SharedMemoryHandle RenderThreadImpl::HostAllocateSharedMemoryBuffer(
    uint32 buffer_size) {
  base::SharedMemoryHandle mem_handle;
  Send(new ChildProcessHostMsg_SyncAllocateSharedMemory(
                buffer_size, &mem_handle));
  return mem_handle;
}

void RenderThreadImpl::RegisterExtension(v8::Extension* extension) {
  WebScriptController::registerExtension(extension);
  v8_extensions_.insert(extension->name());
}

bool RenderThreadImpl::IsRegisteredExtension(
    const std::string& v8_extension_name) const {
  return v8_extensions_.find(v8_extension_name) != v8_extensions_.end();
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
                               content::GetContentClient()->renderer()->
                                   RunIdleHandlerWhenWidgetsHidden();
  if (run_in_foreground_tab) {
    IdleHandlerInForegroundTab();
    return;
  }
#if !defined(OS_MACOSX) && defined(USE_TCMALLOC)
  MallocExtension::instance()->ReleaseFreeMemory();
#endif

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

  // Sample the CPU usage on each timer event, so that it is more accurate.
  double cpu_usage = process_metrics_->GetCPUUsage();

  if (idle_notifications_to_skip_ > 0) {
    idle_notifications_to_skip_--;
  } else if (cpu_usage < kIdleCPUUsageThresholdInPercents) {
    if (v8::V8::IdleNotification()) {
      // V8 finished collecting garbage.
      new_delay_ms = kLongIdleHandlerDelayMs;
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

void RenderThreadImpl::PostponeIdleNotification() {
  idle_notifications_to_skip_ = 2;
}

#if defined(OS_WIN)
void RenderThreadImpl::PreCacheFont(const LOGFONT& log_font) {
  Send(new ChildProcessHostMsg_PreCacheFont(log_font));
}

void RenderThreadImpl::ReleaseCachedFonts() {
  Send(new ChildProcessHostMsg_ReleaseCachedFonts());
}

#endif  // OS_WIN

int32 RenderThreadImpl::RoutingIDForCurrentContext() {
  int32 routing_id = MSG_ROUTING_CONTROL;
  if (v8::Context::InContext()) {
    WebFrame* frame = WebFrame::frameForCurrentContext();
    if (frame) {
      RenderViewImpl* view = RenderViewImpl::FromWebView(frame->view());
      if (view)
        routing_id = view->routing_id();
    }
  } else {
    DLOG(WARNING) << "Not called within a script context!";
  }
  return routing_id;
}

void RenderThreadImpl::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThreadImpl::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

void RenderThreadImpl::OnSetZoomLevelForCurrentURL(const GURL& url,
                                                   double zoom_level) {
  RenderViewZoomer zoomer(url, zoom_level);
  content::RenderView::ForEach(&zoomer);
}

void RenderThreadImpl::OnDOMStorageEvent(
    const DOMStorageMsg_Event_Params& params) {
  if (!dom_storage_event_dispatcher_.get()) {
    EnsureWebKitInitialized();
    dom_storage_event_dispatcher_.reset(WebStorageEventDispatcher::create());
  }
  dom_storage_event_dispatcher_->dispatchStorageEvent(params.key,
      params.old_value, params.new_value, params.origin, params.url,
      params.storage_type == DOM_STORAGE_LOCAL);
}

bool RenderThreadImpl::OnControlMessageReceived(const IPC::Message& msg) {
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
  IPC_BEGIN_MESSAGE_MAP(RenderThreadImpl, msg)
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
    IPC_MESSAGE_HANDLER(ViewMsg_TempCrashWithData, OnTempCrashWithData)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderThreadImpl::OnSetNextPageID(int32 next_page_id) {
  // This is called at process initialization time or when this process is
  // being re-used for a new RenderView.  It is ok if another RenderView
  // has identical page_ids or inflates next_page_id_ just before this arrives,
  // as long as we ensure next_page_id_ is at least this large.
  RenderViewImpl::SetNextPageID(next_page_id);
}

// Called when to register CSS Color name->system color mappings.
// We update the colors one by one and then tell WebKit to refresh all render
// views.
void RenderThreadImpl::OnSetCSSColors(
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

void RenderThreadImpl::OnCreateNewView(const ViewMsg_New_Params& params) {
  EnsureWebKitInitialized();
  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  RenderViewImpl::Create(
      params.parent_window,
      MSG_ROUTING_NONE,
      params.renderer_preferences,
      params.web_preferences,
      new SharedRenderViewCounter(0),
      params.view_id,
      params.session_storage_namespace_id,
      params.frame_name);
}

GpuChannelHost* RenderThreadImpl::EstablishGpuChannelSync(
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
  content::GPUInfo gpu_info;
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
  plugin_refresh_allowed_ = false;
  WebKit::resetPluginCache(reload_pages);
  plugin_refresh_allowed_ = true;
}

void RenderThreadImpl::OnNetworkStateChanged(bool online) {
  EnsureWebKitInitialized();
  WebNetworkStateNotifier::setOnLine(online);
}

void RenderThreadImpl::OnTempCrashWithData(const GURL& data) {
  content::GetContentClient()->SetActiveURL(data);
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
