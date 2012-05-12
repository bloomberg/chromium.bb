// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/utf_string_conversions.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/resource_dispatcher_host_impl.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_util.h"
#include "media/base/media_log_event.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/plugin_constants.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#endif
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using content::BrowserContext;
using content::BrowserMessageFilter;
using content::BrowserThread;
using content::ChildProcessHostImpl;
using content::PluginServiceFilter;
using content::ResourceDispatcherHostImpl;
using content::UserMetricsAction;
using net::CookieStore;

namespace {

const int kPluginsRefreshThresholdInSeconds = 3;

// When two CPU usage queries arrive within this interval, we sample the CPU
// usage only once and send it as a response for both queries.
static const int64 kCPUUsageSampleIntervalMs = 900;

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
  }

  RenderMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

class OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderMessageFilter* filter,
                                   content::ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context) {
  }

  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) {
    *renderer_handle = filter()->peer_handle();
    *renderer_id = filter()->render_process_id();
  }

  virtual void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                                    int plugin_child_id) {
    ViewHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  virtual bool OffTheRecord() {
    return filter()->OffTheRecord();
  }

  virtual content::ResourceContext* GetResourceContext() {
    return context_;
  }

 private:
  content::ResourceContext* context_;
};

class OpenChannelToPpapiBrokerCallback
    : public PpapiPluginProcessHost::BrokerClient {
 public:
  OpenChannelToPpapiBrokerCallback(RenderMessageFilter* filter,
                                   int routing_id,
                                   int request_id)
      : filter_(filter),
        routing_id_(routing_id),
        request_id_(request_id) {
  }

  virtual ~OpenChannelToPpapiBrokerCallback() {}

  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) {
    *renderer_handle = filter_->peer_handle();
    *renderer_id = filter_->render_process_id();
  }

  virtual void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                                    int /* plugin_child_id */) {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        request_id_,
                                                        channel_handle));
    delete this;
  }

  virtual bool OffTheRecord() {
    return filter_->OffTheRecord();
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  int routing_id_;
  int request_id_;
};

}  // namespace

class RenderMessageFilter::OpenChannelToNpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PluginProcessHost::Client {
 public:
  OpenChannelToNpapiPluginCallback(RenderMessageFilter* filter,
                                   content::ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context),
        host_(NULL),
        sent_plugin_channel_request_(false) {
  }

  virtual int ID() OVERRIDE {
    return filter()->render_process_id();
  }

  virtual content::ResourceContext* GetResourceContext() OVERRIDE {
    return context_;
  }

  virtual bool OffTheRecord() OVERRIDE {
    if (filter()->OffTheRecord())
      return true;
    if (content::GetContentClient()->browser()->AllowSaveLocalState(context_))
      return false;

    // For now, only disallow storing data for Flash <http://crbug.com/97319>.
    for (size_t i = 0; i < info_.mime_types.size(); ++i) {
      if (info_.mime_types[i].mime_type == kFlashPluginSwfMimeType)
        return true;
    }
    return false;
  }

  virtual void SetPluginInfo(const webkit::WebPluginInfo& info) OVERRIDE {
    info_ = info;
  }

  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(host);
    host_ = host;
  }

  virtual void OnSentPluginChannelRequest() OVERRIDE {
    sent_plugin_channel_request_ = true;
  }

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    WriteReplyAndDeleteThis(handle);
  }

  virtual void OnError() OVERRIDE {
    WriteReplyAndDeleteThis(IPC::ChannelHandle());
  }

  PluginProcessHost* host() const {
    return host_;
  }

  bool sent_plugin_channel_request() const {
    return sent_plugin_channel_request_;
  }

  void Cancel() {
    delete this;
  }

 private:
  void WriteReplyAndDeleteThis(const IPC::ChannelHandle& handle) {
    ViewHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg(),
                                                      handle, info_);
    filter()->OnCompletedOpenChannelToNpapiPlugin(this);
    SendReplyAndDeleteThis();
  }

  content::ResourceContext* context_;
  webkit::WebPluginInfo info_;
  PluginProcessHost* host_;
  bool sent_plugin_channel_request_;
};

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    content::BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper,
    content::MediaObserver* media_observer)
    : resource_dispatcher_host_(ResourceDispatcherHostImpl::Get()),
      plugin_service_(plugin_service),
      browser_context_(browser_context),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      dom_storage_context_(static_cast<DOMStorageContextImpl*>(
          BrowserContext::GetDOMStorageContext(browser_context))),
      render_process_id_(render_process_id),
      cpu_usage_(0),
      media_observer_(media_observer) {
  DCHECK(request_context_);

  render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(plugin_host_clients_.empty());
}

void RenderMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
  for (std::set<OpenChannelToNpapiPluginCallback*>::iterator it =
       plugin_host_clients_.begin(); it != plugin_host_clients_.end(); ++it) {
    OpenChannelToNpapiPluginCallback* client = *it;
    if (client->host()) {
      if (client->sent_plugin_channel_request()) {
        client->host()->CancelSentRequest(client);
      } else {
        client->host()->CancelPendingRequest(client);
      }
    } else {
      plugin_service_->CancelOpenChannelToNpapiPlugin(client);
    }
    client->Cancel();
  }
  plugin_host_clients_.clear();
}

void RenderMessageFilter::OnChannelConnected(int32 peer_id) {
  BrowserMessageFilter::OnChannelConnected(peer_id);
  base::ProcessHandle handle = peer_handle();
#if defined(OS_MACOSX)
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle,
                                                                    NULL));
#else
  process_metrics_.reset(base::ProcessMetrics::CreateProcessMetrics(handle));
#endif
  cpu_usage_ = process_metrics_->GetCPUUsage(); // Initialize CPU usage counters
  cpu_usage_sample_time_ = base::TimeTicks::Now();
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderMessageFilter, message, *message_was_ok)
#if defined(OS_WIN) && !defined(USE_AURA)
    // On Windows, we handle these on the IO thread to avoid a deadlock with
    // plugins.  On non-Windows systems, we need to handle them on the UI
    // thread.
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetWindowRect, OnGetWindowRect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetRootWindowRect, OnGetRootWindowRect)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GenerateRoutingID, OnGenerateRoutingID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnMsgCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateFullscreenWidget,
                        OnMsgCreateFullscreenWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRawCookies, OnGetRawCookies)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CookiesEnabled, OnCookiesEnabled)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LoadFont, OnLoadFont)
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
        render_widget_helper_->DidReceiveBackingStoreMsg(message))
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateIsDelayed, OnUpdateIsDelayed)
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_SyncAllocateSharedMemory,
                        OnAllocateSharedMemory)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB, OnAllocTransportDIB)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB, OnFreeTransportDIB)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AsyncOpenFile, OnAsyncOpenFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPUUsage, OnGetCPUUsage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareBufferSize,
                        OnGetHardwareBufferSize)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareInputSampleRate,
                        OnGetHardwareInputSampleRate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareSampleRate,
                        OnGetHardwareSampleRate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetHardwareInputChannelLayout,
                        OnGetHardwareInputChannelLayout)
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaLogEvent, OnMediaLogEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool RenderMessageFilter::OffTheRecord() const {
  return incognito_;
}

void RenderMessageFilter::OnMsgCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id,
    int* surface_id,
    int64* cloned_session_storage_namespace_id) {
  bool no_javascript_access;
  bool can_create_window =
      content::GetContentClient()->browser()->CanCreateWindow(
          GURL(params.opener_url),
          GURL(params.opener_security_origin),
          params.window_container_type,
          resource_context_,
          render_process_id_,
          &no_javascript_access);

  if (!can_create_window) {
    *route_id = MSG_ROUTING_NONE;
    *surface_id = 0;
    return;
  }

  // TODO(michaeln): Fix this.
  // This is a bug in the existing impl, session storage is effectively
  // leaked when created thru this code path (window.open()) since there
  // is no balancing DeleteSessionStorage() for this Clone() call anywhere
  // in the codebase. I'm replicating the bug for now.
  *cloned_session_storage_namespace_id =
      dom_storage_context_->LeakyCloneSessionStorage(
          params.session_storage_namespace_id);

  render_widget_helper_->CreateNewWindow(params,
                                         no_javascript_access,
                                         peer_handle(),
                                         route_id,
                                         surface_id);
}

void RenderMessageFilter::OnMsgCreateWidget(int opener_id,
                                            WebKit::WebPopupType popup_type,
                                            int* route_id,
                                            int* surface_id) {
  render_widget_helper_->CreateNewWidget(
      opener_id, popup_type, route_id, surface_id);
}

void RenderMessageFilter::OnMsgCreateFullscreenWidget(int opener_id,
                                                      int* route_id,
                                                      int* surface_id) {
  render_widget_helper_->CreateNewFullscreenWidget(
      opener_id, route_id, surface_id);
}

void RenderMessageFilter::OnSetCookie(const IPC::Message& message,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& cookie) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanUseCookiesForOrigin(render_process_id_, url))
    return;

  net::CookieOptions options;
  if (content::GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie,
          resource_context_, render_process_id_, message.routing_id(),
          &options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    // Pass a null callback since we don't care about when the 'set' completes.
    context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, options, net::CookieMonster::SetCookiesCallback());
  }
}

void RenderMessageFilter::OnGetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies,
                                       IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanUseCookiesForOrigin(render_process_id_, url)) {
    SendGetCookiesResponse(reply_msg, std::string());
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/99242
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  cookie_monster->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderMessageFilter::CheckPolicyForCookies, this, url,
                      first_party_for_cookies, reply_msg));
}

void RenderMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!policy->CanReadRawCookies(render_process_id_) ||
      !policy->CanUseCookiesForOrigin(render_process_id_, url)) {
    SendGetRawCookiesResponse(reply_msg, net::CookieList());
    return;
  }

  // We check policy here to avoid sending back cookies that would not normally
  // be applied to outbound requests for the given URL.  Since this cookie info
  // is visible in the developer tools, it is helpful to make it match reality.
  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  cookie_monster->GetAllCookiesForURLAsync(
      url, base::Bind(&RenderMessageFilter::SendGetRawCookiesResponse,
                      this, reply_msg));
}

void RenderMessageFilter::OnDeleteCookie(const GURL& url,
                                         const std::string& cookie_name) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanUseCookiesForOrigin(render_process_id_, url))
    return;

  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookieAsync(url, cookie_name, base::Closure());
}

void RenderMessageFilter::OnCookiesEnabled(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* cookies_enabled) {
  // TODO(ananta): If this render view is associated with an automation channel,
  // aka ChromeFrame then we need to retrieve cookie settings from the external
  // host.
  *cookies_enabled = content::GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, MSG_ROUTING_CONTROL);
}

#if defined(OS_MACOSX)
void RenderMessageFilter::OnLoadFont(const FontDescriptor& font,
                                     IPC::Message* reply_msg) {
  FontLoader::Result* result = new FontLoader::Result;

  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FontLoader::LoadFont, font, result),
      base::Bind(&RenderMessageFilter::SendLoadFontReply, this, reply_msg,
                 base::Owned(result)));
}

void RenderMessageFilter::SendLoadFontReply(IPC::Message* reply,
                                            FontLoader::Result* result) {
  base::SharedMemoryHandle handle;
  if (result->font_data_size == 0 || result->font_id == 0) {
    result->font_data_size = 0;
    result->font_id = 0;
    handle = base::SharedMemory::NULLHandle();
  } else {
    result->font_data.GiveToProcess(base::GetCurrentProcessHandle(), &handle);
  }
  ViewHostMsg_LoadFont::WriteReplyParams(
      reply, result->font_data_size, handle, result->font_id);
  Send(reply);
}
#endif  // OS_MACOSX

void RenderMessageFilter::OnGetPlugins(
    bool refresh,
    IPC::Message* reply_msg) {
  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeDelta threshold = base::TimeDelta::FromSeconds(
        kPluginsRefreshThresholdInSeconds);
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= threshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      PluginServiceImpl::GetInstance()->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  PluginServiceImpl::GetInstance()->GetPlugins(
      base::Bind(&RenderMessageFilter::GetPluginsCallback, this, reply_msg));
}

void RenderMessageFilter::GetPluginsCallback(
    IPC::Message* reply_msg,
    const std::vector<webkit::WebPluginInfo>& all_plugins) {
  // Filter the plugin list.
  content::PluginServiceFilter* filter =
      PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<webkit::WebPluginInfo> plugins;

  int child_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  for (size_t i = 0; i < all_plugins.size(); ++i) {
    // Copy because the filter can mutate.
    webkit::WebPluginInfo plugin(all_plugins[i]);
    if (!filter || filter->ShouldUsePlugin(child_process_id,
                                           routing_id,
                                           resource_context_,
                                           GURL(),
                                           GURL(),
                                           &plugin)) {
      plugins.push_back(plugin);
    }
  }

  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetPluginInfo(
    int routing_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    bool* found,
    webkit::WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, routing_id, resource_context_,
      url, page_url, mime_type, allow_wildcard,
      NULL, info, actual_mime_type);
}

void RenderMessageFilter::OnOpenChannelToPlugin(int routing_id,
                                                const GURL& url,
                                                const GURL& policy_url,
                                                const std::string& mime_type,
                                                IPC::Message* reply_msg) {
  OpenChannelToNpapiPluginCallback* client =
      new OpenChannelToNpapiPluginCallback(this, resource_context_, reply_msg);
  DCHECK(!ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.insert(client);
  plugin_service_->OpenChannelToNpapiPlugin(
      render_process_id_, routing_id,
      url, policy_url, mime_type, client);
}

void RenderMessageFilter::OnOpenChannelToPepperPlugin(
    const FilePath& path,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      path,
      new OpenChannelToPpapiPluginCallback(
          this, resource_context_, reply_msg));
}

void RenderMessageFilter::OnOpenChannelToPpapiBroker(int routing_id,
                                                     int request_id,
                                                     const FilePath& path) {
  plugin_service_->OpenChannelToPpapiBroker(
      path, new OpenChannelToPpapiBrokerCallback(this, routing_id, request_id));
}

void RenderMessageFilter::OnGenerateRoutingID(int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
}

void RenderMessageFilter::OnGetCPUUsage(int* cpu_usage) {
  base::TimeTicks now = base::TimeTicks::Now();
  int64 since_last_sample_ms = (now - cpu_usage_sample_time_).InMilliseconds();
  if (since_last_sample_ms > kCPUUsageSampleIntervalMs) {
    cpu_usage_sample_time_ = now;
    cpu_usage_ = static_cast<int>(process_metrics_->GetCPUUsage());
  }
  *cpu_usage = cpu_usage_;
}

void RenderMessageFilter::OnGetHardwareBufferSize(uint32* buffer_size) {
  *buffer_size = static_cast<uint32>(media::GetAudioHardwareBufferSize());
}

void RenderMessageFilter::OnGetHardwareInputSampleRate(int* sample_rate) {
  // TODO(henrika): add support for all available input devices.
  *sample_rate = media::GetAudioInputHardwareSampleRate(
      media::AudioManagerBase::kDefaultDeviceId);
}

void RenderMessageFilter::OnGetHardwareSampleRate(int* sample_rate) {
  *sample_rate = media::GetAudioHardwareSampleRate();
}

void RenderMessageFilter::OnGetHardwareInputChannelLayout(
    ChannelLayout* layout) {
  // TODO(henrika): add support for all available input devices.
  *layout = media::GetAudioInputHardwareChannelLayout(
      media::AudioManagerBase::kDefaultDeviceId);
}

void RenderMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                        const GURL& url,
                                        const content::Referrer& referrer,
                                        const string16& suggested_name) {
  content::DownloadSaveInfo save_info;
  save_info.suggested_name = suggested_name;
  scoped_ptr<net::URLRequest> request(new net::URLRequest(url, NULL));
  request->set_referrer(referrer.url.spec());
  webkit_glue::ConfigureURLRequestForReferrerPolicy(
      request.get(), referrer.policy);
  download_stats::RecordDownloadSource(download_stats::INITIATED_BY_RENDERER);
  resource_dispatcher_host_->BeginDownload(
      request.Pass(),
      resource_context_,
      render_process_id_,
      message.routing_id(),
      false,
      save_info,
      ResourceDispatcherHostImpl::DownloadStartedCallback());
}

void RenderMessageFilter::OnCheckNotificationPermission(
    const GURL& source_origin, int* result) {
  *result = content::GetContentClient()->browser()->
      CheckDesktopNotificationPermission(source_origin, resource_context_,
                                         render_process_id_);
}

void RenderMessageFilter::OnAllocateSharedMemory(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  ChildProcessHostImpl::AllocateSharedMemory(
      buffer_size, peer_handle(), handle);
}

net::URLRequestContext* RenderMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::URLRequestContext* context =
      content::GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context_);
  if (!context)
    context = request_context_->GetURLRequestContext();

  return context;
}

#if defined(OS_MACOSX)
void RenderMessageFilter::OnAllocTransportDIB(
    size_t size, bool cache_in_browser, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, cache_in_browser, handle);
}

void RenderMessageFilter::OnFreeTransportDIB(
    TransportDIB::Id dib_id) {
  render_widget_helper_->FreeTransportDIB(dib_id);
}
#endif

bool RenderMessageFilter::CheckPreparsedJsCachingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnablePreparsedJsCaching);
    checked = true;
  }
  return result;
}

void RenderMessageFilter::OnCacheableMetadataAvailable(
    const GURL& url,
    double expected_response_time,
    const std::vector<char>& data) {
  if (!CheckPreparsedJsCachingEnabled())
    return;

  net::HttpCache* cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  DCHECK(cache);

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(data.size()));
  memcpy(buf->data(), &data.front(), data.size());
  cache->WriteMetadata(
      url, base::Time::FromDoubleT(expected_response_time), buf, data.size());
}

void RenderMessageFilter::OnKeygen(uint32 key_size_index,
                                   const std::string& challenge_string,
                                   const GURL& url,
                                   IPC::Message* reply_msg) {
  // Map displayed strings indicating level of keysecurity in the <keygen>
  // menu to the key size in bits. (See SSLKeyGeneratorChromium.cpp in WebCore.)
  int key_size_in_bits;
  switch (key_size_index) {
    case 0:
      key_size_in_bits = 2048;
      break;
    case 1:
      key_size_in_bits = 1024;
      break;
    default:
      DCHECK(false) << "Illegal key_size_index " << key_size_index;
      ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
      Send(reply_msg);
      return;
  }

  VLOG(1) << "Dispatching keygen task to worker pool.";
  // Dispatch to worker pool, so we do not block the IO thread.
  if (!base::WorkerPool::PostTask(
           FROM_HERE,
           base::Bind(
               &RenderMessageFilter::OnKeygenOnWorkerThread, this,
               key_size_in_bits, challenge_string, url, reply_msg),
           true)) {
    NOTREACHED() << "Failed to dispatch keygen task to worker pool";
    ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
    return;
  }
}

void RenderMessageFilter::OnKeygenOnWorkerThread(
    int key_size_in_bits,
    const std::string& challenge_string,
    const GURL& url,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);

  // Generate a signed public key and challenge, then send it back.
  net::KeygenHandler keygen_handler(key_size_in_bits, challenge_string, url);

#if defined(USE_NSS)
  // Attach a password delegate so we can authenticate.
  keygen_handler.set_crypto_module_password_delegate(
      content::GetContentClient()->browser()->GetCryptoPasswordDelegate(url));
#endif  // defined(USE_NSS)

  ViewHostMsg_Keygen::WriteReplyParams(
      reply_msg,
      keygen_handler.GenKeyAndSignChallenge());
  Send(reply_msg);
}

void RenderMessageFilter::OnAsyncOpenFile(const IPC::Message& msg,
                                          const FilePath& path,
                                          int flags,
                                          int message_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!ChildProcessSecurityPolicyImpl::GetInstance()->HasPermissionsForFile(
          render_process_id_, path, flags)) {
    DLOG(ERROR) << "Bad flags in ViewMsgHost_AsyncOpenFile message: " << flags;
    content::RecordAction(UserMetricsAction("BadMessageTerminate_AOF"));
    BadMessageReceived();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(
          &RenderMessageFilter::AsyncOpenFileOnFileThread, this,
          path, flags, message_id, msg.routing_id()));
}

void RenderMessageFilter::AsyncOpenFileOnFileThread(const FilePath& path,
                                                    int flags,
                                                    int message_id,
                                                    int routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  base::PlatformFile file = base::CreatePlatformFile(
      path, flags, NULL, &error_code);
  IPC::PlatformFileForTransit file_for_transit =
      file != base::kInvalidPlatformFileValue ?
          IPC::GetFileHandleForProcess(file, peer_handle(), true) :
          IPC::InvalidPlatformFileForTransit();

  IPC::Message* reply = new ViewMsg_AsyncOpenFile_ACK(
      routing_id, error_code, file_for_transit, message_id);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(base::IgnoreResult(&RenderMessageFilter::Send), this, reply));
}

void RenderMessageFilter::OnMediaLogEvent(const media::MediaLogEvent& event) {
  if (media_observer_)
    media_observer_->OnMediaEvent(render_process_id_, event);
}

void RenderMessageFilter::CheckPolicyForCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (content::GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, reply_msg->routing_id())) {
    // Gets the cookies from cookie store if allowed.
    context->cookie_store()->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&RenderMessageFilter::SendGetCookiesResponse,
                   this, reply_msg));
  } else {
    SendGetCookiesResponse(reply_msg, std::string());
  }
}

void RenderMessageFilter::SendGetCookiesResponse(IPC::Message* reply_msg,
                                                 const std::string& cookies) {
  ViewHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::SendGetRawCookiesResponse(
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  std::vector<webkit_glue::WebCookie> cookies;
  for (size_t i = 0; i < cookie_list.size(); ++i)
    cookies.push_back(webkit_glue::WebCookie(cookie_list[i]));
  ViewHostMsg_GetRawCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::OnCompletedOpenChannelToNpapiPlugin(
    OpenChannelToNpapiPluginCallback* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(ContainsKey(plugin_host_clients_, client));
  plugin_host_clients_.erase(client);
}

void RenderMessageFilter::OnUpdateIsDelayed(const IPC::Message& msg) {
  // When not in accelerated compositing mode, in certain cases (e.g. waiting
  // for a resize or if no backing store) the RenderWidgetHost is blocking the
  // UI thread for some time, waiting for an UpdateRect from the renderer. If we
  // are going to switch to accelerated compositing, the GPU process may need
  // round-trips to the UI thread before finishing the frame, causing deadlocks
  // if we delay the UpdateRect until we receive the OnSwapBuffersComplete. So
  // the renderer sent us this message, so that we can unblock the UI thread.
  // We will simply re-use the UpdateRect unblock mechanism, just with a
  // different message.
  render_widget_helper_->DidReceiveBackingStoreMsg(msg);
}
