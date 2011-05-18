// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_types.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/ppapi_broker_process_host.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/user_metrics.h"
#include "content/common/desktop_notification_messages.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/cookie_monster.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#if defined(OS_MACOSX)
#include "content/common/font_descriptor_mac.h"
#include "content/common/font_loader_mac.h"
#endif
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#if defined(OS_WIN)
#include "content/common/child_process_host.h"
#endif
#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

using net::CookieStore;

namespace {

const int kPluginsRefreshThresholdInSeconds = 3;

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

class ClearCacheCompletion : public RenderMessageCompletionCallback,
                             public net::CompletionCallback {
 public:
  ClearCacheCompletion(RenderMessageFilter* filter,
                       IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    ViewHostMsg_ClearCache::WriteReplyParams(reply_msg(), params.a);
    SendReplyAndDeleteThis();
  }
};

class OpenChannelToNpapiPluginCallback : public RenderMessageCompletionCallback,
                                         public PluginProcessHost::Client {
 public:
  OpenChannelToNpapiPluginCallback(RenderMessageFilter* filter,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg) {
  }

  virtual int ID() {
    return filter()->render_process_id();
  }

  virtual bool OffTheRecord() {
    return filter()->incognito();
  }

  virtual void SetPluginInfo(const webkit::npapi::WebPluginInfo& info) {
    info_ = info;
  }

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) {
    WriteReplyAndDeleteThis(handle);
  }

  virtual void OnError() {
    WriteReplyAndDeleteThis(IPC::ChannelHandle());
  }

 private:
  void WriteReplyAndDeleteThis(const IPC::ChannelHandle& handle) {
    ViewHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg(),
                                                      handle, info_);
    SendReplyAndDeleteThis();
  }

  webkit::npapi::WebPluginInfo info_;
};

class OpenChannelToPpapiPluginCallback : public RenderMessageCompletionCallback,
                                         public PpapiPluginProcessHost::Client {
 public:
  OpenChannelToPpapiPluginCallback(RenderMessageFilter* filter,
                                   const content::ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context) {
  }

  virtual void GetChannelInfo(base::ProcessHandle* renderer_handle,
                              int* renderer_id) {
    *renderer_handle = filter()->peer_handle();
    *renderer_id = filter()->render_process_id();
  }

  virtual void OnChannelOpened(base::ProcessHandle plugin_process_handle,
                               const IPC::ChannelHandle& channel_handle) {
    ViewHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), plugin_process_handle, channel_handle);
    SendReplyAndDeleteThis();
  }

  virtual const content::ResourceContext* GetResourceContext() {
    return context_;
  }

 private:
  const content::ResourceContext* context_;
};

class OpenChannelToPpapiBrokerCallback : public PpapiBrokerProcessHost::Client {
 public:
  OpenChannelToPpapiBrokerCallback(RenderMessageFilter* filter,
                                   int routing_id,
                                   int request_id)
      : filter_(filter),
        routing_id_(routing_id),
        request_id_(request_id) {
  }

  virtual void GetChannelInfo(base::ProcessHandle* renderer_handle,
                              int* renderer_id) {
    *renderer_handle = filter_->peer_handle();
    *renderer_id = filter_->render_process_id();
  }

  virtual void OnChannelOpened(base::ProcessHandle broker_process_handle,
                               const IPC::ChannelHandle& channel_handle) {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        request_id_,
                                                        broker_process_handle,
                                                        channel_handle));
    delete this;
  }

 private:
  scoped_refptr<RenderMessageFilter> filter_;
  int routing_id_;
  int request_id_;
};

// Class to assist with clearing out the cache when we want to preserve
// the sslhostinfo entries.  It's not very efficient, but its just for debug.
class DoomEntriesHelper {
 public:
  explicit DoomEntriesHelper(disk_cache::Backend* backend)
      : backend_(backend),
        entry_(NULL),
        iter_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(this,
            &DoomEntriesHelper::CacheCallback)),
        user_callback_(NULL) {
  }

  void ClearCache(ClearCacheCompletion* callback) {
    user_callback_ = callback;
    return CacheCallback(net::OK);  // Start clearing the cache.
  }

 private:
  void CacheCallback(int result) {
    do {
      if (result != net::OK) {
        user_callback_->RunWithParams(Tuple1<int>(result));
        delete this;
        return;
      }

      if (entry_) {
        // Doom all entries except those with snapstart information.
        std::string key = entry_->GetKey();
        if (key.find("sslhostinfo:") != 0) {
          entry_->Doom();
          backend_->EndEnumeration(&iter_);
          iter_ = NULL;  // We invalidated our iterator - start from the top!
        }
        entry_->Close();
        entry_ = NULL;
      }
      result = backend_->OpenNextEntry(&iter_, &entry_, &callback_);
    } while (result != net::ERR_IO_PENDING);
  }

  disk_cache::Backend* backend_;
  disk_cache::Entry* entry_;
  void* iter_;
  net::CompletionCallbackImpl<DoomEntriesHelper> callback_;
  ClearCacheCompletion* user_callback_;
};

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    PluginService* plugin_service,
    Profile* profile,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper)
    : resource_dispatcher_host_(g_browser_process->resource_dispatcher_host()),
      plugin_service_(plugin_service),
      profile_(profile),
      extension_info_map_(profile->GetExtensionInfoMap()),
      request_context_(request_context),
      resource_context_(profile->GetResourceContext()),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      render_widget_helper_(render_widget_helper),
      notification_prefs_(
          DesktopNotificationServiceFactory::GetForProfile(profile)->
              prefs_cache()),
      host_zoom_map_(profile->GetHostZoomMap()),
      incognito_(profile->IsOffTheRecord()),
      webkit_context_(profile->GetWebKitContext()),
      render_process_id_(render_process_id) {
  DCHECK(request_context_);

  render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void RenderMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                   BrowserThread::ID* thread) {
  switch (message.type()) {
#if defined(USE_X11)
    case ViewHostMsg_GetScreenInfo::ID:
    case ViewHostMsg_GetWindowRect::ID:
    case ViewHostMsg_GetRootWindowRect::ID:
      *thread = BrowserThread::BACKGROUND_X11;
      break;
#endif
    // Can't load plugins on IO thread.
    case ViewHostMsg_GetPlugins::ID:
    // The PluginService::GetPluginInfo may need to load the plugins.  Don't do
    // it on the IO thread.
    case ViewHostMsg_GetPluginInfo::ID:
      *thread = BrowserThread::FILE;
      break;
    default:
      break;
  }
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message,
                                            bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(RenderMessageFilter, message, *message_was_ok)
    // On Linux we need to dispatch these messages to the UI2 thread
    // because we cannot make X calls from the IO thread.  Mac
    // doesn't have windowed plug-ins so we handle the messages in
    // the UI thread.  On Windows, we intercept the messages and
    // handle them directly.
#if !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetScreenInfo, OnGetScreenInfo)
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetRawCookies, OnGetRawCookies)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CookiesEnabled, OnCookiesEnabled)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
#if defined(OS_WIN)  // This hack is Windows-specific.
    IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFont, OnPreCacheFont)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
        render_widget_helper_->DidReceiveUpdateMsg(message))
    IPC_MESSAGE_HANDLER(DesktopNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RevealFolderInOS, OnRevealFolderInOS)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateSharedMemoryBuffer,
                        OnAllocateSharedMemoryBuffer)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB, OnAllocTransportDIB)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB, OnFreeTransportDIB)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseCurrentConnections,
                        OnCloseCurrentConnections)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCacheMode, OnSetCacheMode)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClearHostResolverCache,
                        OnClearHostResolverCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClearPredictorCache, OnClearPredictorCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER(ViewHostMsg_EnableSpdy, OnEnableSpdy)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AsyncOpenFile, OnAsyncOpenFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void RenderMessageFilter::OnRevealFolderInOS(const FilePath& path) {
#if defined(OS_MACOSX)
  const BrowserThread::ID kThreadID = BrowserThread::UI;
#else
  const BrowserThread::ID kThreadID = BrowserThread::FILE;
#endif
  if (!BrowserThread::CurrentlyOn(kThreadID)) {
    // Only honor the request if appropriate persmissions are granted.
    if (ChildProcessSecurityPolicy::GetInstance()->CanReadFile(
          render_process_id_, path)) {
      BrowserThread::PostTask(
          kThreadID, FROM_HERE,
          NewRunnableMethod(
              this, &RenderMessageFilter::OnRevealFolderInOS, path));
    }
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(kThreadID));
  platform_util::OpenItem(path);
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::OnMsgCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id, int64* cloned_session_storage_namespace_id) {
  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (params.window_container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    if (!extension_info_map_->CheckURLAccessToExtensionPermission(
            params.opener_url, Extension::kBackgroundPermission)) {
      *route_id = MSG_ROUTING_NONE;
      return;
    }
  }

  *cloned_session_storage_namespace_id =
      webkit_context_->dom_storage_context()->CloneSessionStorage(
          params.session_storage_namespace_id);
  render_widget_helper_->CreateNewWindow(params,
                                         peer_handle(),
                                         route_id);
}

void RenderMessageFilter::OnMsgCreateWidget(int opener_id,
                                            WebKit::WebPopupType popup_type,
                                            int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, popup_type, route_id);
}

void RenderMessageFilter::OnMsgCreateFullscreenWidget(int opener_id,
                                                      int* route_id) {
  render_widget_helper_->CreateNewFullscreenWidget(opener_id, route_id);
}

void RenderMessageFilter::OnSetCookie(const IPC::Message& message,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& cookie) {
  net::CookieOptions options;
  if (content::GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie,
          resource_context_, render_process_id_, message.routing_id(),
          &options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    context->cookie_store()->SetCookieWithOptions(url, cookie, options);
  }
}

// We use DELAY_REPLY even though we have the result here because we want the
// message's routing_id, and there's no (current) way to get it.
void RenderMessageFilter::OnGetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies,
                                       IPC::Message* reply_msg) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  net::CookieList cookie_list = cookie_monster->GetAllCookiesForURL(url);

  std::string cookies;
  if (content::GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, reply_msg->routing_id())) {
    cookies = context->cookie_store()->GetCookies(url);
  }

  ViewHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::vector<webkit_glue::WebCookie>* cookies) {
  cookies->clear();

  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadRawCookies(
          render_process_id_)) {
    return;
  }

  // We check policy here to avoid sending back cookies that would not normally
  // be applied to outbound requests for the given URL.  Since this cookie info
  // is visible in the developer tools, it is helpful to make it match reality.
  net::URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster =
      context->cookie_store()->GetCookieMonster();
  net::CookieList cookie_list = cookie_monster->GetAllCookiesForURL(url);

  for (size_t i = 0; i < cookie_list.size(); ++i)
    cookies->push_back(webkit_glue::WebCookie(cookie_list[i]));
}

void RenderMessageFilter::OnDeleteCookie(const GURL& url,
                                         const std::string& cookie_name) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookie(url, cookie_name);
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
                                     uint32* handle_size,
                                     base::SharedMemoryHandle* handle) {
  base::SharedMemory font_data;
  uint32 font_data_size = 0;
  bool ok = FontLoader::LoadFontIntoBuffer(font.ToNSFont(), &font_data,
                &font_data_size);
  if (!ok || font_data_size == 0) {
    LOG(ERROR) << "Couldn't load font data for " << font.font_name <<
        " ok=" << ok << " font_data_size=" << font_data_size;
    *handle_size = 0;
    *handle = base::SharedMemory::NULLHandle();
    return;
  }

  *handle_size = font_data_size;
  font_data.GiveToProcess(base::GetCurrentProcessHandle(), handle);
}
#endif  // OS_MACOSX

#if defined(OS_WIN)  // This hack is Windows-specific.
void RenderMessageFilter::OnPreCacheFont(LOGFONT font) {
  ChildProcessHost::PreCacheFont(font);
}
#endif  // OS_WIN

void RenderMessageFilter::OnGetPlugins(
    bool refresh,
    std::vector<webkit::npapi::WebPluginInfo>* plugins) {
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
      if (now - last_plugin_refresh_time_ < threshold)
        refresh = false;  // Ignore refresh request; threshold not exceeded yet.
      else
        last_plugin_refresh_time_ = now;
  }

  webkit::npapi::PluginList::Singleton()->GetEnabledPlugins(refresh, plugins);
}

void RenderMessageFilter::OnGetPluginInfo(
    int routing_id,
    const GURL& url,
    const GURL& policy_url,
    const std::string& mime_type,
    bool* found,
    webkit::npapi::WebPluginInfo* info,
    std::string* actual_mime_type) {
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, routing_id, url, mime_type, info, actual_mime_type);

  if (*found) {
    if (!plugin_service_->PluginAllowedForURL(info->path, policy_url))
      info->enabled |= webkit::npapi::WebPluginInfo::POLICY_DISABLED;
  }
}

void RenderMessageFilter::OnOpenChannelToPlugin(int routing_id,
                                                const GURL& url,
                                                const std::string& mime_type,
                                                IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToNpapiPlugin(
      render_process_id_, routing_id, url, mime_type,
      new OpenChannelToNpapiPluginCallback(this, reply_msg));
}

void RenderMessageFilter::OnOpenChannelToPepperPlugin(
    const FilePath& path,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      path,
      new OpenChannelToPpapiPluginCallback(
          this, &resource_context_, reply_msg));
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

void RenderMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                        const GURL& url,
                                        const GURL& referrer) {
  // Don't show "Save As" UI.
  bool prompt_for_save_location = false;
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           DownloadSaveInfo(),
                                           prompt_for_save_location,
                                           render_process_id_,
                                           message.routing_id(),
                                           resource_context_);
  download_util::RecordDownloadCount(
      download_util::INITIATED_BY_RENDERER_COUNT);
}

void RenderMessageFilter::OnCheckNotificationPermission(
    const GURL& source_url, int* result) {
  *result = WebKit::WebNotificationPresenter::PermissionNotAllowed;

  if (extension_info_map_->CheckURLAccessToExtensionPermission(
          source_url, Extension::kNotificationPermission)) {
    *result = WebKit::WebNotificationPresenter::PermissionAllowed;
    return;
  }

  // Fall back to the regular notification preferences, which works on an
  // origin basis.
  *result = notification_prefs_->HasPermission(source_url.GetOrigin());
}

void RenderMessageFilter::OnAllocateSharedMemoryBuffer(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  if (!shared_buf.CreateAndMapAnonymous(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
    return;
  }
  shared_buf.GiveToProcess(peer_handle(), handle);
}

void RenderMessageFilter::OnDidZoomURL(const IPC::Message& message,
                                       double zoom_level,
                                       bool remember,
                                       const GURL& url) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &RenderMessageFilter::UpdateHostZoomLevelsOnUIThread,
          zoom_level, remember, url, render_process_id_, message.routing_id()));
}

void RenderMessageFilter::UpdateHostZoomLevelsOnUIThread(
    double zoom_level,
    bool remember,
    const GURL& url,
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (remember) {
    host_zoom_map_->SetZoomLevel(url, zoom_level);
    // Notify renderers from this profile.
    for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      RenderProcessHost* render_process_host = i.GetCurrentValue();
      if (render_process_host->profile() == profile_) {
        render_process_host->Send(
            new ViewMsg_SetZoomLevelForCurrentURL(url, zoom_level));
      }
    }
  } else {
    host_zoom_map_->SetTemporaryZoomLevel(
        render_process_id, render_view_id, zoom_level);
  }
}

net::URLRequestContext* RenderMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContextGetter* context_getter =
      url.SchemeIs(chrome::kExtensionScheme) ?
          extensions_request_context_ : request_context_;
  return context_getter->GetURLRequestContext();
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

bool RenderMessageFilter::CheckBenchmarkingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableBenchmarking);
    checked = true;
  }
  return result;
}

void RenderMessageFilter::OnCloseCurrentConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->CloseAllConnections();
}

void RenderMessageFilter::OnSetCacheMode(bool enabled) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;

  net::HttpCache::Mode mode = enabled ?
      net::HttpCache::NORMAL : net::HttpCache::DISABLE;
  net::HttpCache* http_cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  http_cache->set_mode(mode);
}

void RenderMessageFilter::OnClearCache(bool preserve_ssl_host_info,
                                       IPC::Message* reply_msg) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  int rv = -1;
  if (CheckBenchmarkingEnabled()) {
    disk_cache::Backend* backend = request_context_->GetURLRequestContext()->
        http_transaction_factory()->GetCache()->GetCurrentBackend();
    if (backend) {
      ClearCacheCompletion* callback =
          new ClearCacheCompletion(this, reply_msg);
      if (preserve_ssl_host_info) {
        DoomEntriesHelper* helper = new DoomEntriesHelper(backend);
        helper->ClearCache(callback);  // Will self clean.
        return;
      } else {
        rv = backend->DoomAllEntries(callback);
        if (rv == net::ERR_IO_PENDING) {
          // The callback will send the reply.
          return;
        }
        // Completed synchronously, no need for the callback.
        delete callback;
      }
    }
  }
  ViewHostMsg_ClearCache::WriteReplyParams(reply_msg, rv);
  Send(reply_msg);
}

void RenderMessageFilter::OnClearHostResolverCache(int* result) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  *result = -1;
  DCHECK(CheckBenchmarkingEnabled());
  net::HostResolverImpl* host_resolver_impl =
      request_context_->GetURLRequestContext()->
      host_resolver()->GetAsHostResolverImpl();
  if (host_resolver_impl) {
    net::HostCache* cache = host_resolver_impl->cache();
    DCHECK(cache);
    cache->clear();
    *result = 0;
  }
}

void RenderMessageFilter::OnClearPredictorCache(int* result) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  CHECK(CheckBenchmarkingEnabled());
  chrome_browser_net::ClearPredictorCache();
  *result = 0;
}

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

// TODO(lzheng): This only enables spdy over ssl. Enable spdy for http
// when needed.
void RenderMessageFilter::OnEnableSpdy(bool enable) {
  if (enable) {
    net::HttpNetworkLayer::EnableSpdy("npn,force-alt-protocols");
  } else {
    net::HttpNetworkLayer::EnableSpdy("npn-http");
  }
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
           NewRunnableMethod(
               this, &RenderMessageFilter::OnKeygenOnWorkerThread,
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
      browser::NewCryptoModuleBlockingDialogDelegate(
          browser::kCryptoModulePasswordKeygen, url.host()));
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

  if (!ChildProcessSecurityPolicy::GetInstance()->HasPermissionsForFile(
          render_process_id_, path, flags)) {
    DLOG(ERROR) << "Bad flags in ViewMsgHost_AsyncOpenFile message: " << flags;
    UserMetrics::RecordAction(UserMetricsAction("BadMessageTerminate_AOF"));
    BadMessageReceived();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, NewRunnableMethod(
          this, &RenderMessageFilter::AsyncOpenFileOnFileThread,
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
      IPC::InvalidPlatformFileForTransit();
  if (file != base::kInvalidPlatformFileValue) {
#if defined(OS_WIN)
    ::DuplicateHandle(::GetCurrentProcess(), file, peer_handle(),
                      &file_for_transit, 0, false, DUPLICATE_SAME_ACCESS);
#else
    file_for_transit = base::FileDescriptor(file, true);
#endif
  }

  IPC::Message* reply = new ViewMsg_AsyncOpenFile_ACK(
      routing_id, error_code, file_for_transit, message_id);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, NewRunnableMethod(
          this, &RenderMessageFilter::Send, reply));
}
