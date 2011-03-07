// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_thread.h"

#include <algorithm>
#include <limits>
#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/stats_table.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/threading/thread_local.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/appcache/appcache_dispatcher.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/database_messages.h"
#include "chrome/common/db_message_filter.h"
#include "chrome/common/dom_storage_messages.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_localization_peer.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_database_observer_impl.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/automation/dom_automation_v8_extension.h"
#include "chrome/renderer/cookie_message_filter.h"
#include "chrome/renderer/devtools_agent_filter.h"
#include "chrome/renderer/extension_groups.h"
#include "chrome/renderer/extensions/chrome_app_bindings.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/js_only_v8_extensions.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/external_extension.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/gpu_video_service_host.h"
#include "chrome/renderer/indexed_db_dispatcher.h"
#include "chrome/renderer/loadtimes_extension_bindings.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/render_process_impl.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/render_view_visitor.h"
#include "chrome/renderer/renderer_histogram_snapshots.h"
#include "chrome/renderer/renderer_webidbfactory_impl.h"
#include "chrome/renderer/renderer_webkitclient_impl.h"
#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"
#include "chrome/renderer/search_extension.h"
#include "chrome/renderer/searchbox_extension.h"
#include "chrome/renderer/security_filter_peer.h"
#include "chrome/renderer/spellchecker/spellcheck.h"
#include "chrome/renderer/user_script_slave.h"
#include "content/common/resource_dispatcher.h"
#include "content/common/resource_messages.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebColor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCrossOriginPreflightResultCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFontCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/extensions/v8/benchmarking_extension.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/extensions/v8/playback_extension.h"
#include "webkit/glue/webkit_glue.h"
#include "v8/include/v8.h"

// TODO(port)
#if defined(OS_WIN)
#include "chrome/plugin/plugin_channel.h"
#else
#include "base/scoped_handle.h"
#include "chrome/plugin/plugin_channel_base.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#if defined(OS_MACOSX)
#include "chrome/app/breakpad_mac.h"
#endif

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

using WebKit::WebCache;
using WebKit::WebCrossOriginPreflightResultCache;
using WebKit::WebFontCache;
using WebKit::WebFrame;
using WebKit::WebRuntimeFeatures;
using WebKit::WebSecurityPolicy;
using WebKit::WebScriptController;
using WebKit::WebString;
using WebKit::WebStorageEventDispatcher;
using WebKit::WebView;

namespace {
static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;
static const double kInitialIdleHandlerDelayS = 1.0 /* seconds */;
static const double kInitialExtensionIdleHandlerDelayS = 5.0 /* seconds */;
static const int64 kMaxExtensionIdleHandlerDelayS = 5*60 /* seconds */;

// Keep the global RenderThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
static base::LazyInstance<base::ThreadLocalPointer<RenderThread> > lazy_tls(
    base::LINKER_INITIALIZED);

#if defined(OS_POSIX)
class SuicideOnChannelErrorFilter : public IPC::ChannelProxy::MessageFilter {
  void OnChannelError() {
    // On POSIX, at least, one can install an unload handler which loops
    // forever and leave behind a renderer process which eats 100% CPU forever.
    //
    // This is because the terminate signals (ViewMsg_ShouldClose and the error
    // from the IPC channel) are routed to the main message loop but never
    // processed (because that message loop is stuck in V8).
    //
    // One could make the browser SIGKILL the renderers, but that leaves open a
    // large window where a browser failure (or a user, manually terminating
    // the browser because "it's stuck") will leave behind a process eating all
    // the CPU.
    //
    // So, we install a filter on the channel so that we can process this event
    // here and kill the process.

#if defined(OS_MACOSX)
    // TODO(viettrungluu): crbug.com/28547: The following is needed, as a
    // stopgap, to avoid leaking due to not releasing Breakpad properly.
    // TODO(viettrungluu): Investigate why this is being called.
    if (IsCrashReporterEnabled()) {
      VLOG(1) << "Cleaning up Breakpad.";
      DestructCrashReporter();
    } else {
      VLOG(1) << "Breakpad not enabled; no clean-up needed.";
    }
#endif  // OS_MACOSX

    _exit(0);
  }
};
#endif

class RenderViewContentSettingsSetter : public RenderViewVisitor {
 public:
  RenderViewContentSettingsSetter(const GURL& url,
                                  const ContentSettings& content_settings)
      : url_(url),
        content_settings_(content_settings) {
  }

  virtual bool Visit(RenderView* render_view) {
    if (GURL(render_view->webview()->mainFrame()->url()) == url_)
      render_view->SetContentSettings(content_settings_);
    return true;
  }

 private:
  GURL url_;
  ContentSettings content_settings_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewContentSettingsSetter);
};

class RenderViewZoomer : public RenderViewVisitor {
 public:
  RenderViewZoomer(const GURL& url, double zoom_level)
      : zoom_level_(zoom_level) {
    host_ = net::GetHostOrSpecFromURL(url);
  }

  virtual bool Visit(RenderView* render_view) {
    WebView* webview = render_view->webview();  // Guaranteed non-NULL.

    // Don't set zoom level for full-page plugin since they don't use the same
    // zoom settings.
    if (webview->mainFrame()->document().isPluginDocument())
      return true;

    if (net::GetHostOrSpecFromURL(GURL(webview->mainFrame()->url())) == host_)
      webview->setZoomLevel(false, zoom_level_);
    return true;
  }

 private:
  std::string host_;
  double zoom_level_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewZoomer);
};

class RenderResourceObserver : public ResourceDispatcher::Observer {
 public:
  RenderResourceObserver() {
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnRequestComplete(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      ResourceType::Type resource_type,
      const net::URLRequestStatus& status) {
    // Update the browser about our cache.
    RenderThread::current()->InformHostOfCacheStatsLater();

    if (status.status() != net::URLRequestStatus::CANCELED ||
        status.os_error() == net::ERR_ABORTED) {
      return NULL;
    }

    // Resource canceled with a specific error are filtered.
    return SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
        resource_type, current_peer, status.os_error());
  }

  virtual webkit_glue::ResourceLoaderBridge::Peer* OnReceivedResponse(
      webkit_glue::ResourceLoaderBridge::Peer* current_peer,
      const std::string& mime_type,
      const GURL& url) {
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        current_peer, RenderThread::current(), mime_type, url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderResourceObserver);
};

}  // namespace

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
  TRACE_EVENT_BEGIN("RenderThread::Init", 0, "");

  lazy_tls.Pointer()->Set(this);
#if defined(OS_WIN)
  // If you are running plugins in this thread you need COM active but in
  // the normal case you don't.
  if (RenderProcessImpl::InProcessPlugins())
    CoInitialize(0);
#endif

  std::string type_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);
  // In single process the single process is all there is.
  is_extension_process_ = type_str == switches::kExtensionProcess ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
  is_incognito_process_ = false;
  suspend_webkit_shared_timer_ = true;
  notify_webkit_of_modal_loop_ = true;
  plugin_refresh_allowed_ = true;
  cache_stats_task_pending_ = false;
  widget_count_ = 0;
  hidden_widget_count_ = 0;
  idle_notification_delay_in_s_ = is_extension_process_ ?
      kInitialExtensionIdleHandlerDelayS : kInitialIdleHandlerDelayS;
  task_factory_.reset(new ScopedRunnableMethodFactory<RenderThread>(this));

  resource_dispatcher()->set_observer(new RenderResourceObserver());

  visited_link_slave_.reset(new VisitedLinkSlave());
  user_script_slave_.reset(new UserScriptSlave(&extensions_));
  renderer_net_predictor_.reset(new RendererNetPredictor());
  histogram_snapshots_.reset(new RendererHistogramSnapshots());
  appcache_dispatcher_.reset(new AppCacheDispatcher(this));
  indexed_db_dispatcher_.reset(new IndexedDBDispatcher());
  spellchecker_.reset(new SpellCheck());

  devtools_agent_filter_ = new DevToolsAgentFilter();
  AddFilter(devtools_agent_filter_.get());

  db_message_filter_ = new DBMessageFilter();
  AddFilter(db_message_filter_.get());

  cookie_message_filter_ = new CookieMessageFilter();
  AddFilter(cookie_message_filter_.get());

#if defined(OS_POSIX)
  suicide_on_channel_error_filter_ = new SuicideOnChannelErrorFilter;
  AddFilter(suicide_on_channel_error_filter_.get());
#endif

  TRACE_EVENT_END("RenderThread::Init", 0, "");
}

RenderThread::~RenderThread() {
  // Wait for all databases to be closed.
  if (web_database_observer_impl_.get())
    web_database_observer_impl_->WaitForAllDatabasesToClose();

  // Shutdown in reverse of the initialization order.
  RemoveFilter(db_message_filter_.get());
  db_message_filter_ = NULL;
  RemoveFilter(devtools_agent_filter_.get());

  // Shutdown the file thread if it's running.
  if (file_thread_.get())
    file_thread_->Stop();

  if (webkit_client_.get())
    WebKit::shutdown();

  lazy_tls.Pointer()->Set(NULL);

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  PluginChannelBase::CleanupChannels();
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

const ExtensionSet* RenderThread::GetExtensions() const {
  return &extensions_;
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
  bool pumping_events = false, may_show_cookie_prompt = false;
  if (msg->is_sync()) {
    if (msg->is_caller_pumping_messages()) {
      pumping_events = true;
    } else {
      switch (msg->type()) {
        case ViewHostMsg_GetCookies::ID:
        case ViewHostMsg_GetRawCookies::ID:
        case ViewHostMsg_CookiesEnabled::ID:
        case DOMStorageHostMsg_SetItem::ID:
        case ResourceHostMsg_SyncLoad::ID:
        case DatabaseHostMsg_Allow::ID:
          may_show_cookie_prompt = true;
          pumping_events = true;
          break;
      }
    }
  }

  bool suspend_webkit_shared_timer = true;  // default value
  std::swap(suspend_webkit_shared_timer, suspend_webkit_shared_timer_);

  bool notify_webkit_of_modal_loop = true;  // default value
  std::swap(notify_webkit_of_modal_loop, notify_webkit_of_modal_loop_);

  gfx::NativeViewId host_window = 0;

  if (pumping_events) {
    // See ViewMsg_SignalCookiePromptEvent.
    if (may_show_cookie_prompt) {
      static_cast<IPC::SyncMessage*>(msg)->set_pump_messages_event(
          cookie_message_filter_->pump_messages_event());
    }

    if (suspend_webkit_shared_timer)
      webkit_client_->SuspendSharedTimer();

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
      webkit_client_->ResumeSharedTimer();

    // We may end up nesting calls to Send, so we defer the reset until we
    // return to the top-most message loop.
    if (may_show_cookie_prompt &&
        cookie_message_filter_->pump_messages_event()->IsSignaled()) {
      MessageLoop::current()->PostNonNestableTask(FROM_HERE,
          NewRunnableMethod(cookie_message_filter_.get(),
                            &CookieMessageFilter::ResetPumpMessagesEvent));
    }
  }

  return rv;
}

void RenderThread::AddRoute(int32 routing_id,
                            IPC::Channel::Listener* listener) {
  widget_count_++;
  child_process_logging::SetNumberOfViews(widget_count_);
  return ChildThread::AddRoute(routing_id, listener);
}

void RenderThread::RemoveRoute(int32 routing_id) {
  widget_count_--;
  child_process_logging::SetNumberOfViews(widget_count_);
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
  if (!is_extension_process_ &&
      widget_count_ && hidden_widget_count_ == widget_count_)
    ScheduleIdleHandler(kInitialIdleHandlerDelayS);
}

void RenderThread::WidgetRestored() {
  DCHECK_GT(hidden_widget_count_, 0);
  hidden_widget_count_--;
  if (!is_extension_process_)
    idle_timer_.Stop();
}

bool RenderThread::IsExtensionProcess() const {
  return is_extension_process_;
}

bool RenderThread::IsIncognitoProcess() const {
  return is_incognito_process_;
}

void RenderThread::DoNotSuspendWebKitSharedTimer() {
  suspend_webkit_shared_timer_ = false;
}

void RenderThread::DoNotNotifyWebKitOfModalLoop() {
  notify_webkit_of_modal_loop_ = false;
}

void RenderThread::Resolve(const char* name, size_t length) {
  return renderer_net_predictor_->Resolve(name, length);
}

void RenderThread::SendHistograms(int sequence_number) {
  return histogram_snapshots_->SendHistograms(sequence_number);
}

void RenderThread::OnUpdateVisitedLinks(base::SharedMemoryHandle table) {
  DCHECK(base::SharedMemory::IsHandleValid(table)) << "Bad table handle";
  visited_link_slave_->Init(table);
}

void RenderThread::OnAddVisitedLinks(
    const VisitedLinkSlave::Fingerprints& fingerprints) {
  for (size_t i = 0; i < fingerprints.size(); ++i)
    WebView::updateVisitedLinkState(fingerprints[i]);
}

void RenderThread::OnResetVisitedLinks() {
  WebView::resetVisitedLinkState();
}

void RenderThread::OnSetContentSettingsForCurrentURL(
    const GURL& url,
    const ContentSettings& content_settings) {
  RenderViewContentSettingsSetter setter(url, content_settings);
  RenderView::ForEach(&setter);
}

void RenderThread::OnSetZoomLevelForCurrentURL(const GURL& url,
                                               double zoom_level) {
  RenderViewZoomer zoomer(url, zoom_level);
  RenderView::ForEach(&zoomer);
}

void RenderThread::OnUpdateUserScripts(base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
  UpdateActiveExtensions();
}

void RenderThread::OnSetExtensionFunctionNames(
    const std::vector<std::string>& names) {
  ExtensionProcessBindings::SetFunctionNames(names);
}

void RenderThread::OnExtensionLoaded(
    const ViewMsg_ExtensionLoaded_Params& params) {
  scoped_refptr<const Extension> extension(params.ConvertToExtension());
  if (!extension) {
    // This can happen if extension parsing fails for any reason. One reason
    // this can legitimately happen is if the
    // --enable-experimental-extension-apis changes at runtime, which happens
    // during browser tests. Existing renderers won't know about the change.
    return;
  }

  extensions_.Insert(extension);
}

void RenderThread::OnSetExtensionScriptingWhitelist(
    const Extension::ScriptingWhitelist& extension_ids) {
  Extension::SetScriptingWhitelist(extension_ids);
}

void RenderThread::OnExtensionUnloaded(const std::string& id) {
  extensions_.Remove(id);
}

void RenderThread::OnPageActionsUpdated(
    const std::string& extension_id,
    const std::vector<std::string>& page_actions) {
  ExtensionProcessBindings::SetPageActions(extension_id, page_actions);
}

void RenderThread::OnExtensionSetAPIPermissions(
    const std::string& extension_id,
    const std::set<std::string>& permissions) {
  ExtensionProcessBindings::SetAPIPermissions(extension_id, permissions);

  // This is called when starting a new extension page, so start the idle
  // handler ticking.
  ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayS);

  UpdateActiveExtensions();
}

void RenderThread::OnExtensionSetHostPermissions(
    const GURL& extension_url, const std::vector<URLPattern>& permissions) {
  ExtensionProcessBindings::SetHostPermissions(extension_url, permissions);
}

void RenderThread::OnDOMStorageEvent(
    const DOMStorageMsg_Event_Params& params) {
  if (!dom_storage_event_dispatcher_.get())
    dom_storage_event_dispatcher_.reset(WebStorageEventDispatcher::create());
  dom_storage_event_dispatcher_->dispatchStorageEvent(params.key,
      params.old_value, params.new_value, params.origin, params.url,
      params.storage_type == DOM_STORAGE_LOCAL);
}

bool RenderThread::OnControlMessageReceived(const IPC::Message& msg) {
  // Some messages are handled by delegates.
  if (appcache_dispatcher_->OnMessageReceived(msg))
    return true;
  if (indexed_db_dispatcher_->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderThread, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_VisitedLink_NewTable, OnUpdateVisitedLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_VisitedLink_Add, OnAddVisitedLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_VisitedLink_Reset, OnResetVisitedLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_SetContentSettingsForCurrentURL,
                        OnSetContentSettingsForCurrentURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetZoomLevelForCurrentURL,
                        OnSetZoomLevelForCurrentURL)
    IPC_MESSAGE_HANDLER(ViewMsg_SetIsIncognitoProcess, OnSetIsIncognitoProcess)
    IPC_MESSAGE_HANDLER(ViewMsg_SetNextPageID, OnSetNextPageID)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCSSColors, OnSetCSSColors)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRendererHistograms,
                        OnGetRendererHistograms)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRendererTcmalloc,
                        OnGetRendererTcmalloc)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_GetV8HeapStats, OnGetV8HeapStats)
    IPC_MESSAGE_HANDLER(ViewMsg_GetCacheResourceStats,
                        OnGetCacheResourceStats)
    IPC_MESSAGE_HANDLER(ViewMsg_UserScripts_UpdatedScripts,
                        OnUpdateUserScripts)
    // TODO(rafaelw): create an ExtensionDispatcher that handles extension
    // messages seperates their handling from the RenderThread.
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionMessageInvoke,
                        OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_SetFunctionNames,
                        OnSetExtensionFunctionNames)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionLoaded,
                        OnExtensionLoaded)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionUnloaded,
                        OnExtensionUnloaded)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_SetScriptingWhitelist,
                        OnSetExtensionScriptingWhitelist)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgeMemory, OnPurgeMemory)
    IPC_MESSAGE_HANDLER(ViewMsg_PurgePluginListCache,
                        OnPurgePluginListCache)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_UpdatePageActions,
                        OnPageActionsUpdated)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_SetAPIPermissions,
                        OnExtensionSetAPIPermissions)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_SetHostPermissions,
                        OnExtensionSetHostPermissions)
    IPC_MESSAGE_HANDLER(DOMStorageMsg_Event,
                        OnDOMStorageEvent)
#if defined(IPC_MESSAGE_LOG_ENABLED)
    IPC_MESSAGE_HANDLER(ViewMsg_SetIPCLoggingEnabled,
                        OnSetIPCLoggingEnabled)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_SpellChecker_Init,
                        OnInitSpellChecker)
    IPC_MESSAGE_HANDLER(ViewMsg_SpellChecker_WordAdded,
                        OnSpellCheckWordAdded)
    IPC_MESSAGE_HANDLER(ViewMsg_SpellChecker_EnableAutoSpellCorrect,
                        OnSpellCheckEnableAutoSpellCorrect)
    IPC_MESSAGE_HANDLER(ViewMsg_GpuChannelEstablished, OnGpuChannelEstablished)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPhishingModel, OnSetPhishingModel)
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

void RenderThread::OnSetCacheCapacities(size_t min_dead_capacity,
                                        size_t max_dead_capacity,
                                        size_t capacity) {
  EnsureWebKitInitialized();
  WebCache::setCapacities(
      min_dead_capacity, max_dead_capacity, capacity);
}

void RenderThread::OnClearCache() {
  EnsureWebKitInitialized();
  WebCache::clear();
}

void RenderThread::OnGetCacheResourceStats() {
  EnsureWebKitInitialized();
  WebCache::ResourceTypeStats stats;
  WebCache::getResourceTypeStats(&stats);
  Send(new ViewHostMsg_ResourceTypeStats(stats));
}

void RenderThread::OnGetRendererHistograms(int sequence_number) {
  SendHistograms(sequence_number);
}

#if defined(USE_TCMALLOC)
void RenderThread::OnGetRendererTcmalloc() {
  std::string result;
  char buffer[1024 * 32];
  base::ProcessId pid = base::GetCurrentProcId();
  MallocExtension::instance()->GetStats(buffer, sizeof(buffer));
  result.append(buffer);
  Send(new ViewHostMsg_RendererTcmalloc(pid, result));
}
#endif

void RenderThread::OnGetV8HeapStats() {
  v8::HeapStatistics heap_stats;
  v8::V8::GetHeapStatistics(&heap_stats);
  Send(new ViewHostMsg_V8HeapStats(heap_stats.total_heap_size(),
                                   heap_stats.used_heap_size()));
}

void RenderThread::InformHostOfCacheStats() {
  EnsureWebKitInitialized();
  WebCache::UsageStats stats;
  WebCache::getUsageStats(&stats);
  Send(new ViewHostMsg_UpdatedCacheStats(stats));
  cache_stats_task_pending_ = false;
}

void RenderThread::InformHostOfCacheStatsLater() {
  // Rate limit informing the host of our cache stats.
  if (cache_stats_task_pending_)
    return;

  cache_stats_task_pending_ = true;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      task_factory_->NewRunnableMethod(
          &RenderThread::InformHostOfCacheStats),
      kCacheStatsDelayMS);
}

void RenderThread::CloseCurrentConnections() {
  Send(new ViewHostMsg_CloseCurrentConnections());
}

void RenderThread::SetCacheMode(bool enabled) {
  Send(new ViewHostMsg_SetCacheMode(enabled));
}

void RenderThread::ClearCache(bool preserve_ssl_host_info) {
  int rv;
  Send(new ViewHostMsg_ClearCache(preserve_ssl_host_info, &rv));
}

void RenderThread::EnableSpdy(bool enable) {
  Send(new ViewHostMsg_EnableSpdy(enable));
}

void RenderThread::UpdateActiveExtensions() {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess))
    return;

  std::set<std::string> active_extensions;
  user_script_slave_->GetActiveExtensions(&active_extensions);
  ExtensionProcessBindings::GetActiveExtensions(&active_extensions);
  child_process_logging::SetActiveExtensions(active_extensions);
}

void RenderThread::EstablishGpuChannel() {
  if (gpu_channel_.get()) {
    // Do nothing if we already have a GPU channel or are already
    // establishing one.
    if (gpu_channel_->state() == GpuChannelHost::kUnconnected ||
        gpu_channel_->state() == GpuChannelHost::kConnected)
      return;

    // Recreate the channel if it has been lost.
    if (gpu_channel_->state() == GpuChannelHost::kLost)
      gpu_channel_ = NULL;
  }

  if (!gpu_channel_.get())
    gpu_channel_ = new GpuChannelHost;

  // Ask the browser for the channel name.
  Send(new GpuHostMsg_EstablishGpuChannel());
}

GpuChannelHost* RenderThread::EstablishGpuChannelSync() {
  EstablishGpuChannel();
  Send(new GpuHostMsg_SynchronizeGpu());
  return GetGpuChannel();
}

GpuChannelHost* RenderThread::GetGpuChannel() {
  if (!gpu_channel_.get())
    return NULL;

  if (gpu_channel_->state() != GpuChannelHost::kConnected)
    return NULL;

  return gpu_channel_.get();
}

static void* CreateHistogram(
    const char *name, int min, int max, size_t buckets) {
  if (min <= 0)
    min = 1;
  scoped_refptr<base::Histogram> histogram = base::Histogram::FactoryGet(
      name, min, max, buckets, base::Histogram::kUmaTargetedHistogramFlag);
  // We'll end up leaking these histograms, unless there is some code hiding in
  // there to do the dec-ref.
  // TODO(jar): Handle reference counting in webkit glue.
  histogram->AddRef();
  return histogram.get();
}

static void AddHistogramSample(void* hist, int sample) {
  base::Histogram* histogram = static_cast<base::Histogram*>(hist);
  histogram->Add(sample);
}

void RenderThread::EnsureWebKitInitialized() {
  if (webkit_client_.get())
    return;

  // For extensions, we want to ensure we call the IdleHandler every so often,
  // even if the extension keeps up activity.
  if (is_extension_process_) {
    forced_idle_timer_.Start(
        base::TimeDelta::FromSeconds(kMaxExtensionIdleHandlerDelayS),
        this, &RenderThread::IdleHandler);
  }

  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);
  v8::V8::SetCreateHistogramFunction(CreateHistogram);
  v8::V8::SetAddHistogramSampleFunction(AddHistogramSample);

  webkit_client_.reset(new RendererWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());

  WebScriptController::enableV8SingleThreadMode();

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  webkit_glue::EnableWebCoreLogChannels(
      command_line.GetSwitchValueASCII(switches::kWebCoreLogChannels));

  // chrome: pages should not be accessible by normal content, and should
  // also be unable to script anything but themselves (to help limit the damage
  // that a corrupt chrome: page could cause).
  WebString chrome_ui_scheme(ASCIIToUTF16(chrome::kChromeUIScheme));
  WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(chrome_ui_scheme);

  // chrome-extension: resources shouldn't trigger insecure content warnings.
  WebString extension_scheme(ASCIIToUTF16(chrome::kExtensionScheme));
  WebSecurityPolicy::registerURLSchemeAsSecure(extension_scheme);

#if defined(OS_WIN)
  // We don't yet support Gears on non-Windows, so don't tell pages that we do.
  RegisterExtension(extensions_v8::GearsExtension::Get(), false);
#endif
  RegisterExtension(extensions_v8::LoadTimesExtension::Get(), false);
  RegisterExtension(extensions_v8::ChromeAppExtension::Get(), false);
  RegisterExtension(extensions_v8::ExternalExtension::Get(), false);
  RegisterExtension(extensions_v8::SearchBoxExtension::Get(), false);
  v8::Extension* search_extension = extensions_v8::SearchExtension::Get();
  // search_extension is null if not enabled.
  if (search_extension)
    RegisterExtension(search_extension, false);

  if (command_line.HasSwitch(switches::kEnableBenchmarking))
    RegisterExtension(extensions_v8::BenchmarkingExtension::Get(), false);

  if (command_line.HasSwitch(switches::kPlaybackMode) ||
      command_line.HasSwitch(switches::kRecordMode) ||
      command_line.HasSwitch(switches::kNoJsRandomness)) {
    RegisterExtension(extensions_v8::PlaybackExtension::Get(), false);
  }

  if (command_line.HasSwitch(switches::kDomAutomationController))
    RegisterExtension(DomAutomationV8Extension::Get(), false);

  // Add v8 extensions related to chrome extensions.
  RegisterExtension(ExtensionProcessBindings::Get(), true);
  RegisterExtension(BaseJsV8Extension::Get(), true);
  RegisterExtension(JsonSchemaJsV8Extension::Get(), true);
  RegisterExtension(EventBindings::Get(), true);
  RegisterExtension(RendererExtensionBindings::Get(), true);
  RegisterExtension(ExtensionApiTestV8Extension::Get(), true);

  web_database_observer_impl_.reset(new WebDatabaseObserverImpl(this));
  WebKit::WebDatabase::setObserver(web_database_observer_impl_.get());

  WebRuntimeFeatures::enableMediaPlayer(
      RenderProcess::current()->HasInitializedMediaLibrary());

  WebRuntimeFeatures::enableSockets(
      !command_line.HasSwitch(switches::kDisableWebSockets));

  WebRuntimeFeatures::enableDatabase(
      !command_line.HasSwitch(switches::kDisableDatabases));

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

  WebRuntimeFeatures::enableWebAudio(
      command_line.HasSwitch(switches::kEnableWebAudio));

  WebRuntimeFeatures::enableWebGL(
      !command_line.HasSwitch(switches::kDisable3DAPIs) &&
      !command_line.HasSwitch(switches::kDisableExperimentalWebGL));

  WebRuntimeFeatures::enablePushState(true);

  WebRuntimeFeatures::enableTouch(
      command_line.HasSwitch(switches::kEnableTouch));

  WebRuntimeFeatures::enableDeviceMotion(
      command_line.HasSwitch(switches::kEnableDeviceMotion));

  WebRuntimeFeatures::enableDeviceOrientation(
      !command_line.HasSwitch(switches::kDisableDeviceOrientation));

  WebRuntimeFeatures::enableSpeechInput(
      !command_line.HasSwitch(switches::kDisableSpeechInput));

  WebRuntimeFeatures::enableFileSystem(
      !command_line.HasSwitch(switches::kDisableFileSystem));

  WebRuntimeFeatures::enableJavaScriptI18NAPI(
      command_line.HasSwitch(switches::kEnableJavaScriptI18NAPI));
}

void RenderThread::IdleHandler() {
#if (defined(OS_WIN) || defined(OS_LINUX)) && defined(USE_TCMALLOC)
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
  if (is_extension_process_) {
    // Dampen the forced delay as well if the extension stays idle for long
    // periods of time.
    int64 forced_delay_s =
        std::max(static_cast<int64>(idle_notification_delay_in_s_),
                 kMaxExtensionIdleHandlerDelayS);
    forced_idle_timer_.Stop();
    forced_idle_timer_.Start(
        base::TimeDelta::FromSeconds(forced_delay_s),
        this, &RenderThread::IdleHandler);
  }
}

void RenderThread::ScheduleIdleHandler(double initial_delay_s) {
  idle_notification_delay_in_s_ = initial_delay_s;
  idle_timer_.Stop();
  idle_timer_.Start(
      base::TimeDelta::FromSeconds(static_cast<int64>(initial_delay_s)),
      this, &RenderThread::IdleHandler);
}

void RenderThread::OnExtensionMessageInvoke(const std::string& extension_id,
                                            const std::string& function_name,
                                            const ListValue& args,
                                            const GURL& event_url) {
  RendererExtensionBindings::Invoke(
      extension_id, function_name, args, NULL, event_url);

  // Reset the idle handler each time there's any activity like event or message
  // dispatch, for which Invoke is the chokepoint.
  if (is_extension_process_)
    ScheduleIdleHandler(kInitialExtensionIdleHandlerDelayS);
}

void RenderThread::OnPurgeMemory() {
  spellchecker_.reset(new SpellCheck());

  EnsureWebKitInitialized();

  // Clear the object cache (as much as possible; some live objects cannot be
  // freed).
  WebCache::clear();

  // Clear the font/glyph cache.
  WebFontCache::clear();

  // Clear the Cross-Origin Preflight cache.
  WebCrossOriginPreflightResultCache::clear();

  // Release all freeable memory from the SQLite process-global page cache (a
  // low-level object which backs the Connection-specific page caches).
  while (sqlite3_release_memory(std::numeric_limits<int>::max()) > 0) {
  }

  // Repeatedly call the V8 idle notification until it returns true ("nothing
  // more to free").  Note that it makes more sense to do this than to implement
  // a new "delete everything" pass because object references make it difficult
  // to free everything possible in just one pass.
  while (!v8::V8::IdleNotification()) {
  }

#if (defined(OS_WIN) || defined(OS_LINUX)) && defined(USE_TCMALLOC)
  // Tell tcmalloc to release any free pages it's still holding.
  MallocExtension::instance()->ReleaseFreeMemory();
#endif
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

void RenderThread::OnInitSpellChecker(
    IPC::PlatformFileForTransit bdict_file,
    const std::vector<std::string>& custom_words,
    const std::string& language,
    bool auto_spell_correct) {
  spellchecker_->Init(IPC::PlatformFileForTransitToPlatformFile(bdict_file),
                      custom_words, language);
  spellchecker_->EnableAutoSpellCorrect(auto_spell_correct);
}

void RenderThread::OnSpellCheckWordAdded(const std::string& word) {
  spellchecker_->WordAdded(word);
}

void RenderThread::OnSpellCheckEnableAutoSpellCorrect(bool enable) {
  spellchecker_->EnableAutoSpellCorrect(enable);
}

void RenderThread::OnSetIsIncognitoProcess(bool is_incognito_process) {
  is_incognito_process_ = is_incognito_process;
}

void RenderThread::OnGpuChannelEstablished(
    const IPC::ChannelHandle& channel_handle,
    base::ProcessHandle renderer_process_for_gpu,
    const GPUInfo& gpu_info) {
  gpu_channel_->set_gpu_info(gpu_info);

  if (!channel_handle.name.empty()) {
    // Connect to the GPU process if a channel name was received.
    gpu_channel_->Connect(channel_handle, renderer_process_for_gpu);
  } else {
    // Otherwise cancel the connection.
    gpu_channel_ = NULL;
  }
}

void RenderThread::OnSetPhishingModel(IPC::PlatformFileForTransit model_file) {
  safe_browsing::PhishingClassifierDelegate::SetPhishingModel(model_file);
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

bool RenderThread::AllowScriptExtension(const std::string& v8_extension_name,
                                        const GURL& url,
                                        int extension_group) {
  // If we don't know about it, it was added by WebCore, so we should allow it.
  if (v8_extensions_.find(v8_extension_name) == v8_extensions_.end())
    return true;

  // If the V8 extension is not restricted, allow it to run anywhere.
  bool restrict_to_extensions = v8_extensions_[v8_extension_name];
  if (!restrict_to_extensions)
    return true;

  // Extension-only bindings should be restricted to content scripts and
  // extension-blessed URLs.
  if (extension_group == EXTENSION_GROUP_CONTENT_SCRIPTS ||
      extensions_.ExtensionBindingsAllowed(url)) {
    return true;
  }

  return false;
}

void RenderThread::RegisterExtension(v8::Extension* extension,
                                     bool restrict_to_extensions) {
  WebScriptController::registerExtension(extension);
  v8_extensions_[extension->name()] = restrict_to_extensions;
}
