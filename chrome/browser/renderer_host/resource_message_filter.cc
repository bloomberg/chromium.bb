// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "app/clipboard/clipboard.h"
#include "app/gfx/native_widget_types.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_file_util.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/nacl_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_ui.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/database_dispatcher_host.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/task_manager.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/appcache/appcache_dispatcher_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/histogram_synchronizer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/worker_messages.h"
#include "net/base/cookie_monster.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webplugin.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#elif defined(OS_LINUX) || defined(OS_FREEBSD)
// TODO(port) remove this.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

using WebKit::WebCache;

namespace {

// Context menus are somewhat complicated. We need to intercept them here on
// the I/O thread to add any spelling suggestions to them. After that's done,
// we need to forward the modified message to the UI thread and the normal
// message forwarding isn't set up for sending modified messages.
//
// Therefore, this class dispatches the IPC message to the RenderProcessHost
// with the given ID (if possible) to emulate the normal dispatch.
class ContextMenuMessageDispatcher : public Task {
 public:
  ContextMenuMessageDispatcher(
      int render_process_id,
      const ViewHostMsg_ContextMenu& context_menu_message)
      : render_process_id_(render_process_id),
        context_menu_message_(context_menu_message) {
  }

  void Run() {
    RenderProcessHost* host =
        RenderProcessHost::FromID(render_process_id_);
    if (host)
      host->OnMessageReceived(context_menu_message_);
  }

 private:
  int render_process_id_;
  const ViewHostMsg_ContextMenu context_menu_message_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuMessageDispatcher);
};

// Completes a clipboard write initiated by the renderer. The write must be
// performed on the UI thread because the clipboard service from the IO thread
// cannot create windows so it cannot be the "owner" of the clipboard's
// contents.
class WriteClipboardTask : public Task {
 public:
  explicit WriteClipboardTask(Clipboard::ObjectMap* objects)
      : objects_(objects) {}
  ~WriteClipboardTask() {}

  void Run() {
    g_browser_process->clipboard()->WriteObjects(*objects_.get());
  }

 private:
  scoped_ptr<Clipboard::ObjectMap> objects_;
};

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   ViewMsg_Print_Params* params) {
  DCHECK(params);
#if defined(OS_WIN) || defined(OS_MACOSX)
  params->printable_size.SetSize(
      settings.page_setup_pixels().content_area().width(),
      settings.page_setup_pixels().content_area().height());
  params->dpi = settings.dpi();
  // Currently hardcoded at 1.25. See PrintSettings' constructor.
  params->min_shrink = settings.min_shrink;
  // Currently hardcoded at 2.0. See PrintSettings' constructor.
  params->max_shrink = settings.max_shrink;
  // Currently hardcoded at 72dpi. See PrintSettings' constructor.
  params->desired_dpi = settings.desired_dpi;
  // Always use an invalid cookie.
  params->document_cookie = 0;
  params->selection_only = settings.selection_only;
#else
  NOTIMPLEMENTED();
#endif
}

Blacklist::Match* GetPrivacyBlacklistMatchForURL(
    const GURL& url, ChromeURLRequestContext* context) {
  const Blacklist* blacklist = context->GetPrivacyBlacklist();
  // TODO(phajdan.jr): DCHECK(blacklist_manager) when blacklists are stable.
  if (!blacklist)
    return NULL;
  return blacklist->FindMatch(url);
}

}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    int child_id,
    AudioRendererHost* audio_renderer_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper,
    URLRequestContextGetter* request_context)
    : Receiver(RENDER_PROCESS, child_id),
      channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      request_context_(request_context),
      media_request_context_(profile->GetRequestContextForMedia()),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      extensions_message_service_(profile->GetExtensionMessageService()),
      render_widget_helper_(render_widget_helper),
      audio_renderer_host_(audio_renderer_host),
      appcache_dispatcher_host_(
      new AppCacheDispatcherHost(profile->GetRequestContext())),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_storage_dispatcher_host_(
          new DOMStorageDispatcherHost(this, profile->GetWebKitContext(),
              resource_dispatcher_host->webkit_thread()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(db_dispatcher_host_(
          new DatabaseDispatcherHost(profile->GetDatabaseTracker(), this))),
      notification_prefs_(
          profile->GetDesktopNotificationService()->prefs_cache()),
      host_zoom_map_(profile->GetHostZoomMap()),
      off_the_record_(profile->IsOffTheRecord()),
      next_route_id_callback_(NewCallbackWithReturnValue(
          render_widget_helper, &RenderWidgetHelper::GetNextRoutingID)),
      ALLOW_THIS_IN_INITIALIZER_LIST(translation_service_(this)) {
  DCHECK(request_context_);
  DCHECK(media_request_context_);
  DCHECK(audio_renderer_host_.get());
  DCHECK(appcache_dispatcher_host_.get());
  DCHECK(dom_storage_dispatcher_host_.get());

  render_widget_helper_->Init(id(), resource_dispatcher_host_);
}

ResourceMessageFilter::~ResourceMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Tell the DOM Storage dispatcher host to stop sending messages via us.
  dom_storage_dispatcher_host_->Shutdown();

  // Shut down the database dispatcher host.
  db_dispatcher_host_->Shutdown();

  // Let interested observers know we are being deleted.
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());

  if (handle())
    base::CloseProcessHandle(handle());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;

  // Add the observers to intercept.
  registrar_.Add(this, NotificationType::BLACKLIST_NONVISUAL_RESOURCE_BLOCKED,
                 NotificationService::AllSources());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelConnected(int32 peer_pid) {
  DCHECK(!handle()) << " " << handle();
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  base::ProcessHandle peer_handle;
  if (!base::OpenProcessHandle(peer_pid, &peer_handle)) {
    NOTREACHED();
  }
  set_handle(peer_handle);

  // Hook AudioRendererHost to this object after channel is connected so it can
  // this object for sending messages.
  audio_renderer_host_->IPCChannelConnected(id(), handle(), this);

  WorkerService::GetInstance()->Initialize(resource_dispatcher_host_);
  appcache_dispatcher_host_->Initialize(this, id(), handle());
  dom_storage_dispatcher_host_->Init(handle());
  db_dispatcher_host_->Init(handle());
}

void ResourceMessageFilter::OnChannelError() {
  NotificationService::current()->Notify(
      NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
      Source<ResourceMessageFilter>(this),
      NotificationService::NoDetails());
}

// Called on the IPC thread:
void ResourceMessageFilter::OnChannelClosing() {
  channel_ = NULL;

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  resource_dispatcher_host_->CancelRequestsForProcess(id());

  // Unhook AudioRendererHost.
  audio_renderer_host_->IPCChannelClosing();
}

// Called on the IPC thread:
bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& msg) {
  MessagePortDispatcher* mp_dispatcher = MessagePortDispatcher::GetInstance();
  bool msg_is_ok = true;
  bool handled =
      resource_dispatcher_host_->OnMessageReceived(msg, this, &msg_is_ok) ||
      appcache_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      dom_storage_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      audio_renderer_host_->OnMessageReceived(msg, &msg_is_ok) ||
      db_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      mp_dispatcher->OnMessageReceived(
          msg, this, next_route_id_callback(), &msg_is_ok);

  if (!handled) {
    DCHECK(msg_is_ok);  // It should have been marked handled if it wasn't OK.
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(ResourceMessageFilter, msg, msg_is_ok)
      // On Linux we need to dispatch these messages to the UI2 thread
      // because we cannot make X calls from the IO thread.  Mac
      // doesn't have windowed plug-ins so we handle the messages in
      // the UI thread.  On Windows, we intercept the messages and
      // handle them directly.
#if !defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetScreenInfo,
                                      OnGetScreenInfo)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetWindowRect,
                                      OnGetWindowRect)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRootWindowRect,
                                      OnGetRootWindowRect)
#endif

      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnMsgCreateWindow)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookies, OnGetCookies)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetRawCookies, OnGetRawCookies)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCookiesEnabled, OnGetCookiesEnabled)
#if defined(OS_WIN)  // This hack is Windows-specific.
      IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginPath, OnGetPluginPath)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
                                  OnReceiveContextMenuMsg(msg))
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                      OnOpenChannelToPlugin)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LaunchNaCl, OnLaunchNaCl)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_LookupSharedWorker, OnLookupSharedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CancelCreateDedicatedWorker,
                          OnCancelCreateDedicatedWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker,
                          OnForwardToWorker)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_PlatformCheckSpelling,
                          OnPlatformCheckSpelling)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SpellChecker_PlatformFillSuggestionList,
                          OnPlatformFillSuggestionList)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDocumentTag,
                                      OnGetDocumentTag)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentWithTagClosed,
                          OnDocumentWithTagClosed)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ShowSpellingPanel, OnShowSpellingPanel)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateSpellingPanelWithMisspelledWord,
                          OnUpdateSpellingPanelWithMisspelledWord)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DnsPrefetch, OnDnsPrefetch)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererHistograms,
                          OnRendererHistograms)
      IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
          render_widget_helper_->DidReceiveUpdateMsg(msg))
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsAsync,
                          OnClipboardWriteObjects)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsSync,
                          OnClipboardWriteObjects)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardIsFormatAvailable,
                                      OnClipboardIsFormatAvailable)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadText,
                                      OnClipboardReadText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadAsciiText,
                                      OnClipboardReadAsciiText)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadHTML,
                                      OnClipboardReadHTML)
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardFindPboardWriteStringAsync,
                          OnClipboardFindPboardWriteString)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_CheckNotificationPermission,
                          OnCheckNotificationPermission)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromExtension,
                          OnGetMimeTypeFromExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetMimeTypeFromFile,
                          OnGetMimeTypeFromFile)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPreferredExtensionForMimeType,
                          OnGetPreferredExtensionForMimeType)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                          OnGetCPBrowsingContext)
#if defined(OS_WIN)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_LINUX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_AllocateTempFileForPrinting,
                                      OnAllocateTempFileForPrinting)
      IPC_MESSAGE_HANDLER(ViewHostMsg_TempFileForPrintingWritten,
                          OnTempFileForPrintingWritten)
#endif
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocatePDFTransport,
                          OnAllocatePDFTransport)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_V8HeapStats, OnV8HeapStats)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomHost, OnDidZoomHost)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                      OnGetDefaultPrintSettings)
#if defined(OS_WIN) || defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                      OnScriptedPrint)
#endif
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB,
                          OnAllocTransportDIB)
      IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB,
                          OnFreeTransportDIB)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToExtension,
                          OnOpenChannelToExtension)
      IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToTab, OnOpenChannelToTab)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CloseCurrentConnections,
                          OnCloseCurrentConnections)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCacheMode, OnSetCacheMode)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileSize, OnGetFileSize)
      IPC_MESSAGE_HANDLER(ViewHostMsg_Keygen, OnKeygen)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetExtensionMessageBundle,
                                      OnGetExtensionMessageBundle)
      IPC_MESSAGE_HANDLER(ViewHostMsg_TranslateText, OnTranslateText)
#if defined(USE_TCMALLOC)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererTcmalloc, OnRendererTcmalloc)
#endif
      IPC_MESSAGE_UNHANDLED(
          handled = false)
    IPC_END_MESSAGE_MAP_EX()
  }

  if (!msg_is_ok)
    BrowserRenderProcessHost::BadMessageTerminateProcess(msg.type(), handle());

  return handled;
}

void ResourceMessageFilter::OnDestruct() {
  ChromeThread::DeleteOnIOThread::Destruct(this);
}

void ResourceMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ContextMenuParams params;
  if (!IPC::ParamTraits<ContextMenuParams>::Read(&msg, &iter, &params))
    return;

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      new ContextMenuMessageDispatcher(id(), context_menu_message));
}

// Called on the IPC thread:
bool ResourceMessageFilter::Send(IPC::Message* message) {
  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

URLRequestContext* ResourceMessageFilter::GetRequestContext(
    uint32 request_id,
    const ViewHostMsg_Resource_Request& request_data) {
  URLRequestContextGetter* request_context = request_context_;
  // If the request has resource type of ResourceType::MEDIA, we use a request
  // context specific to media for handling it because these resources have
  // specific needs for caching.
  if (request_data.resource_type == ResourceType::MEDIA)
    request_context = media_request_context_;
  return request_context->GetURLRequestContext();
}

void ResourceMessageFilter::OnMsgCreateWindow(
    int opener_id, bool user_gesture, int64 session_storage_namespace_id,
    int* route_id, int64* cloned_session_storage_namespace_id) {
  *cloned_session_storage_namespace_id = dom_storage_dispatcher_host_->
      CloneSessionStorage(session_storage_namespace_id);
  render_widget_helper_->CreateNewWindow(opener_id,
                                         user_gesture,
                                         handle(),
                                         route_id);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id,
                                              bool activatable,
                                              int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, activatable, route_id);
}

void ResourceMessageFilter::OnSetCookie(const GURL& url,
                                        const GURL& first_party_for_cookies,
                                        const std::string& cookie) {
  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  if (!context->cookie_policy()->CanSetCookie(url, first_party_for_cookies))
    return;
  scoped_ptr<Blacklist::Match> match(
      GetPrivacyBlacklistMatchForURL(url, context));
  if (match.get() && (match->attributes() & Blacklist::kBlockCookies))
    return;
  context->cookie_store()->SetCookie(url, cookie);
}

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         std::string* cookies) {
  URLRequestContext* context = GetRequestContextForURL(url);
  if (context->cookie_policy()->CanGetCookies(url, first_party_for_cookies))
    *cookies = context->cookie_store()->GetCookies(url);
}

void ResourceMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::vector<webkit_glue::WebCookie>* raw_cookies) {
  raw_cookies->clear();

  URLRequestContext* context = GetRequestContextForURL(url);
  net::CookieMonster* cookie_monster = context->cookie_store()->
      GetCookieMonster();
  if (!cookie_monster) {
    NOTREACHED();
    return;
  }

  if (!context->cookie_policy()->CanGetCookies(url, first_party_for_cookies))
    return;

  typedef std::vector<net::CookieMonster::CanonicalCookie> CanonicalCookieList;
  CanonicalCookieList cookies;
  cookie_monster->GetRawCookies(url, &cookies);
  for (CanonicalCookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
     raw_cookies->push_back(
         webkit_glue::WebCookie(
             it->Name(),
             it->Value(),
             url.host(),
             it->Path(),
             it->ExpiryDate().ToDoubleT() * 1000,
             it->IsHttpOnly(),
             it->IsSecure(),
             !it->IsPersistent()));
  }
}

void ResourceMessageFilter::OnDeleteCookie(const GURL& url,
                                           const std::string& cookie_name) {
  URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookie(url, cookie_name);
}

void ResourceMessageFilter::OnGetCookiesEnabled(
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* enabled) {
  URLRequestContext* context = GetRequestContextForURL(url);
  *enabled =
      context->cookie_policy()->type() != net::CookiePolicy::BLOCK_ALL_COOKIES;
}

#if defined(OS_WIN)  // This hack is Windows-specific.
void ResourceMessageFilter::OnLoadFont(LOGFONT font) {
  // If renderer is running in a sandbox, GetTextMetrics
  // can sometimes fail. If a font has not been loaded
  // previously, GetTextMetrics will try to load the font
  // from the font file. However, the sandboxed renderer does
  // not have permissions to access any font files and
  // the call fails. So we make the browser pre-load the
  // font for us by using a dummy call to GetTextMetrics of
  // the same font.

  // Maintain a circular queue for the fonts and DCs to be cached.
  // font_index maintains next available location in the queue.
  static const int kFontCacheSize = 32;
  static HFONT fonts[kFontCacheSize] = {0};
  static HDC hdcs[kFontCacheSize] = {0};
  static size_t font_index = 0;

  UMA_HISTOGRAM_COUNTS_100("Memory.CachedFontAndDC",
      fonts[kFontCacheSize-1] ? kFontCacheSize : static_cast<int>(font_index));

  HDC hdc = GetDC(NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  TEXTMETRIC tm;
  BOOL ret = GetTextMetrics(hdc, &tm);
  DCHECK(ret);

  if (fonts[font_index] || hdcs[font_index]) {
    // We already have too many fonts, we will delete one and take it's place.
    DeleteObject(fonts[font_index]);
    ReleaseDC(NULL, hdcs[font_index]);
  }

  fonts[font_index] = font_handle;
  hdcs[font_index] = hdc;
  font_index = (font_index + 1) % kFontCacheSize;
}
#endif

void ResourceMessageFilter::OnGetPlugins(bool refresh,
                                         IPC::Message* reply_msg) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetPluginsOnFileThread, refresh,
          reply_msg));
}

void ResourceMessageFilter::OnGetPluginsOnFileThread(
    bool refresh, IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(refresh, &plugins);
  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnGetPluginPath(const GURL& url,
                                            const GURL& policy_url,
                                            const std::string& mime_type,
                                            FilePath* filename,
                                            std::string* url_mime_type) {
  *filename = plugin_service_->GetPluginPath(
      url, policy_url, mime_type, url_mime_type);
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::wstring& locale,
                                                  IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(
      this, url, mime_type, locale, reply_msg);
}

void ResourceMessageFilter::OnLaunchNaCl(
    const std::wstring& url, int channel_descriptor, IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(resource_dispatcher_host_, url);
  host->Launch(this, channel_descriptor, reply_msg);
}

void ResourceMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params, int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
  WorkerService::GetInstance()->CreateWorker(
      params.url, params.is_shared, off_the_record(), params.name,
      params.document_id, id(), params.render_view_route_id, this, *route_id);
}

void ResourceMessageFilter::OnLookupSharedWorker(const GURL& url,
                                                 const string16& name,
                                                 unsigned long long document_id,
                                                 int render_view_route_id,
                                                 int* route_id,
                                                 bool* url_mismatch) {
  int new_route_id = render_widget_helper_->GetNextRoutingID();
  bool worker_found = WorkerService::GetInstance()->LookupSharedWorker(
      url, name, off_the_record(), document_id, id(), render_view_route_id,
      this, new_route_id, url_mismatch);
  *route_id = worker_found ? new_route_id : MSG_ROUTING_NONE;
}

void ResourceMessageFilter::OnDocumentDetached(unsigned long long document_id) {
  // Notify the WorkerService that the passed document was detached so any
  // associated shared workers can be shut down.
  WorkerService::GetInstance()->DocumentDetached(this, document_id);
}

void ResourceMessageFilter::OnCancelCreateDedicatedWorker(int route_id) {
  WorkerService::GetInstance()->CancelCreateDedicatedWorker(this, route_id);
}

void ResourceMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerService::GetInstance()->ForwardMessage(message, this);
}

void ResourceMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                          const GURL& url,
                                          const GURL& referrer) {
  URLRequestContext* context = request_context_->GetURLRequestContext();
  FilePath save_file_path;
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           save_file_path,
                                           id(),
                                           message.routing_id(),
                                           context);
}

void ResourceMessageFilter::OnClipboardWriteObjects(
    const Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

#if defined(OS_WIN)
  // We pass the renderer handle to assist the clipboard with using shared
  // memory objects. handle() is a handle to the process that would
  // own any shared memory that might be in the object list. We only do this
  // on Windows and it only applies to bitmaps. (On Linux, bitmaps
  // are copied pixel by pixel rather than using shared memory.)
  Clipboard::DuplicateRemoteHandles(handle(), long_living_objects);
#endif

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE, new WriteClipboardTask(long_living_objects));
}

#if !defined(OS_LINUX)
// On non-Linux platforms, clipboard actions can be performed on the IO thread.
// On Linux, since the clipboard is linked with GTK, we either have to do this
// with GTK on the UI thread, or with Xlib on the BACKGROUND_X11 thread. In an
// ideal world, we would do the latter. However, for now we're going to
// terminate these calls on the UI thread. This risks deadlock in the case of
// plugins, but it's better than crashing which is what doing on the IO thread
// gives us.
//
// See resource_message_filter_gtk.cc for the Linux implementation of these
// functions.

void ResourceMessageFilter::OnClipboardIsFormatAvailable(
    Clipboard::FormatType format, Clipboard::Buffer buffer,
    IPC::Message* reply) {
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);
  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadText(Clipboard::Buffer buffer,
                                                IPC::Message* reply) {
  string16 result;
  GetClipboard()->ReadText(buffer, &result);
  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadAsciiText(Clipboard::Buffer buffer,
                                                     IPC::Message* reply) {
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);
  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply, result);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadHTML(Clipboard::Buffer buffer,
                                                IPC::Message* reply) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply, markup, src_url);
  Send(reply);
}

#endif

void ResourceMessageFilter::OnCheckNotificationPermission(
    const GURL& source_origin, int* result) {
  *result = notification_prefs_->HasPermission(source_origin);
}

void ResourceMessageFilter::OnGetMimeTypeFromExtension(
    const FilePath::StringType& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void ResourceMessageFilter::OnGetMimeTypeFromFile(
    const FilePath& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

void ResourceMessageFilter::OnGetPreferredExtensionForMimeType(
    const std::string& mime_type, FilePath::StringType* ext) {
  net::GetPreferredExtensionForMimeType(mime_type, ext);
}

void ResourceMessageFilter::OnGetCPBrowsingContext(uint32* context) {
  // Always allocate a new context when a plugin requests one, since it needs to
  // be unique for that plugin instance.
  *context = CPBrowsingContextManager::Instance()->Allocate(
      request_context_->GetURLRequestContext());
}

#if defined(OS_WIN)
void ResourceMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, handle());
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}
#endif

#if defined(OS_MACOSX)
void ResourceMessageFilter::OnAllocatePDFTransport(
    size_t buffer_size,
    base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  shared_buf.Create(L"", false, false, buffer_size);
  if (!shared_buf.Map(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map PDF transport buffer";
    return;
  }
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), handle);
}
#endif

void ResourceMessageFilter::OnResourceTypeStats(
    const WebCache::ResourceTypeStats& stats) {
  HISTOGRAM_COUNTS("WebCoreCache.ImagesSizeKB",
                   static_cast<int>(stats.images.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.CSSStylesheetsSizeKB",
                   static_cast<int>(stats.cssStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.ScriptsSizeKB",
                   static_cast<int>(stats.scripts.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.XSLStylesheetsSizeKB",
                   static_cast<int>(stats.xslStyleSheets.size / 1024));
  HISTOGRAM_COUNTS("WebCoreCache.FontsSizeKB",
                   static_cast<int>(stats.fonts.size / 1024));
  // We need to notify the TaskManager of these statistics from the UI
  // thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(
          &ResourceMessageFilter::OnResourceTypeStatsOnUIThread,
          stats,
          base::GetProcId(handle())));
}

void ResourceMessageFilter::OnResourceTypeStatsOnUIThread(
    WebCache::ResourceTypeStats stats, base::ProcessId renderer_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TaskManager::GetInstance()->model()->NotifyResourceTypeStats(
      renderer_id, stats);
}


void ResourceMessageFilter::OnV8HeapStats(int v8_memory_allocated,
                                          int v8_memory_used) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(&ResourceMessageFilter::OnV8HeapStatsOnUIThread,
                          v8_memory_allocated,
                          v8_memory_used,
                          base::GetProcId(handle())));
}

// static
void ResourceMessageFilter::OnV8HeapStatsOnUIThread(
    int v8_memory_allocated, int v8_memory_used, base::ProcessId renderer_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TaskManager::GetInstance()->model()->NotifyV8HeapStats(
      renderer_id,
      static_cast<size_t>(v8_memory_allocated),
      static_cast<size_t>(v8_memory_used));
}

void ResourceMessageFilter::OnDidZoomHost(const std::string& host,
                                          int zoom_level) {
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread,
                        host, zoom_level));
}

void ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread(
    const std::string& host,
    int zoom_level) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  host_zoom_map_->SetZoomLevel(host, zoom_level);

  // Notify renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (render_process_host->profile() == profile_) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentHost(host, zoom_level));
    }
  }
}

void ResourceMessageFilter::OnResolveProxy(const GURL& url,
                                           IPC::Message* reply_msg) {
  resolve_proxy_msg_helper_.Start(url, reply_msg);
}

void ResourceMessageFilter::OnResolveProxyCompleted(
    IPC::Message* reply_msg,
    int result,
    const std::string& proxy_list) {
  ViewHostMsg_ResolveProxy::WriteReplyParams(reply_msg, result, proxy_list);
  Send(reply_msg);
}

void ResourceMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             false,
                             task);
}

void ResourceMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If user hasn't cancelled.
  if (printer_query->cookie() && printer_query->settings().dpi()) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#if defined(OS_WIN) || defined(OS_MACOSX)

void ResourceMessageFilter::OnScriptedPrint(
    const ViewHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
#if defined(OS_WIN)
  HWND host_window = gfx::NativeViewFromId(params.host_window_id);
#elif defined(OS_MACOSX)
  gfx::NativeWindow host_window = NULL;
  // TODO: Get an actual window ref here, to allow a sheet-based print dialog.
#endif

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(params.cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &ResourceMessageFilter::OnScriptedPrintReply,
      printer_query,
      params.routing_id,
      reply_msg);
#if defined(OS_WIN)
  // Shows the Print... dialog box. This is asynchronous, only the IPC message
  // sender will hang until the Print dialog is dismissed.
  if (!host_window || !IsWindow(host_window)) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    host_window = GetDesktopWindow();
  } else {
    host_window = GetAncestor(host_window, GA_ROOTOWNER);
  }
  DCHECK(host_window);
#endif

  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_window,
                             params.expected_pages_count,
                             params.has_selection,
                             task);
}

void ResourceMessageFilter::OnScriptedPrintReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    int routing_id,
    IPC::Message* reply_msg) {
  ViewMsg_PrintPages_Params params;
  if (printer_query->last_status() != printing::PrintingContext::OK ||
      !printer_query->settings().dpi()) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params.params);
    params.params.document_cookie = printer_query->cookie();
    params.pages =
        printing::PageRange::GetPages(printer_query->settings().ranges);
  }
  ViewHostMsg_ScriptedPrint::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  if (params.params.dpi && params.params.document_cookie) {
    print_job_manager_->QueuePrinterQuery(printer_query.get());
  } else {
    printer_query->StopWorker();
  }
}

#endif  // OS_WIN || OS_MACOSX

// static
Clipboard* ResourceMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static Clipboard* clipboard = new Clipboard;

  return clipboard;
}

ChromeURLRequestContext*
ResourceMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  URLRequestContextGetter* context_getter =
      url.SchemeIs(chrome::kExtensionScheme) ?
          extensions_request_context_ : request_context_;
  return static_cast<ChromeURLRequestContext*>(
      context_getter->GetURLRequestContext());
}

void ResourceMessageFilter::OnPlatformCheckSpelling(const string16& word,
                                                    int tag,
                                                    bool* correct) {
  *correct = SpellCheckerPlatform::CheckSpelling(word, tag);
}

void ResourceMessageFilter::OnPlatformFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  SpellCheckerPlatform::FillSuggestionList(word, suggestions);
}

void ResourceMessageFilter::OnGetDocumentTag(IPC::Message* reply_msg) {
  int tag = SpellCheckerPlatform::GetDocumentTag();
  ViewHostMsg_GetDocumentTag::WriteReplyParams(reply_msg, tag);
  Send(reply_msg);
  return;
}

void ResourceMessageFilter::OnDocumentWithTagClosed(int tag) {
  SpellCheckerPlatform::CloseDocumentWithTag(tag);
}

void ResourceMessageFilter::OnShowSpellingPanel(bool show) {
  SpellCheckerPlatform::ShowSpellingPanel(show);
}

void ResourceMessageFilter::OnUpdateSpellingPanelWithMisspelledWord(
    const string16& word) {
  SpellCheckerPlatform::UpdateSpellingPanelWithMisspelledWord(word);
}

void ResourceMessageFilter::Observe(NotificationType type,
                                    const NotificationSource &source,
                                    const NotificationDetails &details) {
  if (type == NotificationType::BLACKLIST_NONVISUAL_RESOURCE_BLOCKED) {
    BlacklistUI::OnNonvisualContentBlocked(
        Details<const URLRequest>(details).ptr());
  }
}

void ResourceMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}

void ResourceMessageFilter::OnRendererHistograms(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer::DeserializeHistogramList(sequence_number, histograms);
}

#if defined(OS_MACOSX)
void ResourceMessageFilter::OnAllocTransportDIB(
    size_t size, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, handle);
}

void ResourceMessageFilter::OnFreeTransportDIB(
    TransportDIB::Id dib_id) {
  render_widget_helper_->FreeTransportDIB(dib_id);
}
#endif

void ResourceMessageFilter::OnOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  if (extensions_message_service_.get()) {
    *port_id = extensions_message_service_->
        OpenChannelToExtension(routing_id, source_extension_id,
                               target_extension_id, channel_name, this);
  } else {
    *port_id = -1;
  }
}

void ResourceMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  if (extensions_message_service_.get()) {
    *port_id = extensions_message_service_->
        OpenChannelToTab(routing_id, tab_id, extension_id, channel_name, this);
  } else {
    *port_id = -1;
  }
}

bool ResourceMessageFilter::CheckBenchmarkingEnabled() {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableBenchmarking);
    checked = true;
  }
  return result;
}

void ResourceMessageFilter::OnCloseCurrentConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->CloseCurrentConnections();
}

void ResourceMessageFilter::OnSetCacheMode(bool enabled) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled())
    return;

  net::HttpCache::Mode mode = enabled ?
      net::HttpCache::NORMAL : net::HttpCache::DISABLE;
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->set_mode(mode);
}

void ResourceMessageFilter::OnGetFileSize(const FilePath& path,
                                          IPC::Message* reply_msg) {
  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanUploadFile(id(), path)) {
    ViewHostMsg_GetFileSize::WriteReplyParams(
        reply_msg, static_cast<int64>(-1));
    Send(reply_msg);
    return;
  }

  // Getting file size could take long time if it lives on a network share,
  // so run it on FILE thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetFileSizeOnFileThread, path,
          reply_msg));
}

void ResourceMessageFilter::OnGetFileSizeOnFileThread(
    const FilePath& path, IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  int64 result;
  if (!file_util::GetFileSize(path, &result))
    result = -1;
  ViewHostMsg_GetFileSize::WriteReplyParams(reply_msg, result);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnKeygen(uint32 key_size_index,
                                     const std::string& challenge_string,
                                     const GURL& url,
                                     std::string* signed_public_key) {
  scoped_ptr<net::KeygenHandler> keygen_handler(
      new net::KeygenHandler(key_size_index,
                             challenge_string));
  *signed_public_key = keygen_handler->GenKeyAndSignChallenge();
}

void ResourceMessageFilter::OnTranslateText(
    ViewHostMsg_TranslateTextParam param) {
  translation_service_.Translate(param.routing_id, param.work_id,
                                 param.text_chunks, param.from_language,
                                 param.to_language, param.secure);
}

#if defined(USE_TCMALLOC)
void ResourceMessageFilter::OnRendererTcmalloc(base::ProcessId pid,
                                               const std::string& output) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(AboutTcmallocRendererCallback, pid, output));
}
#endif

void ResourceMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
    request_context_->GetURLRequestContext());

  FilePath extension_path = context->GetPathForExtension(extension_id);
  std::string default_locale =
    context->GetDefaultLocaleForExtension(extension_id);

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          extension_path, default_locale, reply_msg));
}

void ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const FilePath& extension_path,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  std::string error;
  scoped_ptr<ExtensionMessageBundle> bundle(
      extension_file_util::LoadExtensionMessageBundle(
          extension_path, default_locale, &error));

  std::map<std::string, std::string> dictionary_map;
  if (bundle.get())
    dictionary_map = *bundle->dictionary();

  ViewHostMsg_GetExtensionMessageBundle::WriteReplyParams(
      reply_msg, dictionary_map);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}
