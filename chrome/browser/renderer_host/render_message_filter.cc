// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_message_filter.h"

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/sys_string_conversions.h"
#include "base/threading/worker_pool.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/clipboard_dispatcher.h"
#include "chrome/browser/download/download_types.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/ppapi_plugin_process_host.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "ipc/ipc_channel_handle.h"
#include "net/base/cookie_monster.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/plugin_selection_policy.h"
#endif
#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/task_helpers.h"
#include "chrome/common/font_descriptor_mac.h"
#include "chrome/common/font_loader_mac.h"
#endif
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#if defined(OS_WIN)
#include "chrome/common/child_process_host.h"
#endif
#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif
#if defined(USE_TCMALLOC)
#include "chrome/browser/browser_about_handler.h"
#endif

using net::CookieStore;
using WebKit::WebCache;

namespace {

const int kPluginsRefreshThresholdInSeconds = 3;

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
  explicit WriteClipboardTask(ui::Clipboard::ObjectMap* objects)
      : objects_(objects) {}
  ~WriteClipboardTask() {}

  void Run() {
    g_browser_process->clipboard()->WriteObjects(*objects_.get());
  }

 private:
  scoped_ptr<ui::Clipboard::ObjectMap> objects_;
};

void RenderParamsFromPrintSettings(const printing::PrintSettings& settings,
                                   ViewMsg_Print_Params* params) {
  DCHECK(params);
  params->page_size = settings.page_setup_device_units().physical_size();
  params->printable_size.SetSize(
      settings.page_setup_device_units().content_area().width(),
      settings.page_setup_device_units().content_area().height());
  params->margin_top = settings.page_setup_device_units().content_area().x();
  params->margin_left = settings.page_setup_device_units().content_area().y();
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
  params->supports_alpha_blend = settings.supports_alpha_blend();
}

class ClearCacheCompletion : public net::CompletionCallback {
 public:
  ClearCacheCompletion(IPC::Message* reply_msg,
                       RenderMessageFilter* filter)
      : reply_msg_(reply_msg),
        filter_(filter) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    ViewHostMsg_ClearCache::WriteReplyParams(reply_msg_, params.a);
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  IPC::Message* reply_msg_;
  scoped_refptr<RenderMessageFilter> filter_;
};

class OpenChannelToPluginCallback : public PluginProcessHost::Client {
 public:
  OpenChannelToPluginCallback(RenderMessageFilter* filter,
                              IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual int ID() {
    return filter_->render_process_id();
  }

  virtual bool OffTheRecord() {
    return filter_->off_the_record();
  }

  virtual void SetPluginInfo(const webkit::npapi::WebPluginInfo& info) {
    info_ = info;
  }

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) {
    WriteReply(handle);
  }

  virtual void OnError() {
    WriteReply(IPC::ChannelHandle());
  }

 private:
  void WriteReply(const IPC::ChannelHandle& handle) {
    ViewHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg_,
                                                      handle,
                                                      info_);
    filter_->Send(reply_msg_);
    delete this;
  }

  scoped_refptr<RenderMessageFilter> filter_;
  IPC::Message* reply_msg_;
  webkit::npapi::WebPluginInfo info_;
};

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    PluginService* plugin_service,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper)
    : resource_dispatcher_host_(g_browser_process->resource_dispatcher_host()),
      plugin_service_(plugin_service),
      print_job_manager_(g_browser_process->print_job_manager()),
      profile_(profile),
      content_settings_(profile->GetHostContentSettingsMap()),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      render_widget_helper_(render_widget_helper),
      notification_prefs_(
          profile->GetDesktopNotificationService()->prefs_cache()),
      host_zoom_map_(profile->GetHostZoomMap()),
      off_the_record_(profile->IsOffTheRecord()),
      webkit_context_(profile->GetWebKitContext()),
      render_process_id_(render_process_id) {
  request_context_ = profile_->GetRequestContext();
  DCHECK(request_context_);

  render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
#if defined(OS_CHROMEOS)
  cloud_print_enabled_ = true;
#else
  cloud_print_enabled_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrint);
#endif
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

// Called on the IPC thread:
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetScreenInfo, OnGetScreenInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetWindowRect, OnGetWindowRect)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRootWindowRect,
                                    OnGetRootWindowRect)
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_CookiesEnabled,
                                    OnCookiesEnabled)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
#if defined(OS_WIN)  // This hack is Windows-specific.
    IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFont, OnPreCacheFont)
#endif
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_ContextMenu,
                                OnReceiveContextMenuMsg(message))
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPlugin,
                                    OnOpenChannelToPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_LaunchNaCl, OnLaunchNaCl)
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
    IPC_MESSAGE_HANDLER(ViewHostMsg_RendererHistograms, OnRendererHistograms)
    IPC_MESSAGE_HANDLER_GENERIC(ViewHostMsg_UpdateRect,
        render_widget_helper_->DidReceiveUpdateMsg(message))
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsAsync,
                        OnClipboardWriteObjectsAsync)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ClipboardWriteObjectsSync,
                        OnClipboardWriteObjectsSync)
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadAvailableTypes,
                                    OnClipboardReadAvailableTypes)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadData,
                                    OnClipboardReadData)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClipboardReadFilenames,
                                    OnClipboardReadFilenames)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CheckNotificationPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RevealFolderInOS, OnRevealFolderInOS)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetCPBrowsingContext,
                        OnGetCPBrowsingContext)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DuplicateSection, OnDuplicateSection)
#endif
#if defined(OS_POSIX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocateSharedMemoryBuffer,
                        OnAllocateSharedMemoryBuffer)
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_AllocateTempFileForPrinting,
                                    OnAllocateTempFileForPrinting)
    IPC_MESSAGE_HANDLER(ViewHostMsg_TempFileForPrintingWritten,
                        OnTempFileForPrintingWritten)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_ResourceTypeStats, OnResourceTypeStats)
    IPC_MESSAGE_HANDLER(ViewHostMsg_V8HeapStats, OnV8HeapStats)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidZoomURL, OnDidZoomURL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ResolveProxy, OnResolveProxy)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetDefaultPrintSettings,
                                    OnGetDefaultPrintSettings)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint, OnScriptedPrint)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewHostMsg_AllocTransportDIB, OnAllocTransportDIB)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FreeTransportDIB, OnFreeTransportDIB)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToExtension,
                        OnOpenChannelToExtension)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenChannelToTab, OnOpenChannelToTab)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CloseCurrentConnections,
                        OnCloseCurrentConnections)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SetCacheMode, OnSetCacheMode)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER(ViewHostMsg_EnableSpdy, OnEnableSpdy)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetExtensionMessageBundle,
                                    OnGetExtensionMessageBundle)
#if defined(USE_TCMALLOC)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RendererTcmalloc, OnRendererTcmalloc)
#endif
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

void RenderMessageFilter::OnReceiveContextMenuMsg(const IPC::Message& msg) {
  void* iter = NULL;
  ContextMenuParams params;
  if (!IPC::ParamTraits<ContextMenuParams>::Read(&msg, &iter, &params))
    return;

  // Create a new ViewHostMsg_ContextMenu message.
  const ViewHostMsg_ContextMenu context_menu_message(msg.routing_id(), params);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      new ContextMenuMessageDispatcher(
          render_process_id_, context_menu_message));
}

void RenderMessageFilter::OnMsgCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id, int64* cloned_session_storage_namespace_id) {
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

void RenderMessageFilter::OnMsgCreateFullscreenWidget(
    int opener_id, WebKit::WebPopupType popup_type, int* route_id) {
  render_widget_helper_->CreateNewFullscreenWidget(
      opener_id, popup_type, route_id);
}

void RenderMessageFilter::OnSetCookie(const IPC::Message& message,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& cookie) {
  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  SetCookieCompletion* callback = new SetCookieCompletion(
      render_process_id_, message.routing_id(), url, cookie, context);

  // If this render view is associated with an automation channel, aka
  // ChromeFrame then we need to set cookies in the external host.
  if (!AutomationResourceMessageFilter::SetCookiesForUrl(url,
                                                         cookie,
                                                         callback)) {
    int policy = net::OK;
    if (context->cookie_policy()) {
      policy = context->cookie_policy()->CanSetCookie(
          url, first_party_for_cookies, cookie, callback);
      if (policy == net::ERR_IO_PENDING)
        return;
    }
    callback->Run(policy);
  }
}

void RenderMessageFilter::OnGetCookies(const GURL& url,
                                       const GURL& first_party_for_cookies,
                                       IPC::Message* reply_msg) {
  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  GetCookiesCompletion* callback = new GetCookiesCompletion(
      render_process_id_, reply_msg->routing_id(), url, reply_msg, this,
      context, false);

  // If this render view is associated with an automation channel, aka
  // ChromeFrame then we need to retrieve cookies from the external host.
  if (!AutomationResourceMessageFilter::GetCookiesForUrl(url, callback)) {
    int policy = net::OK;
    if (context->cookie_policy()) {
      policy = context->cookie_policy()->CanGetCookies(
          url, first_party_for_cookies, callback);
      if (policy == net::ERR_IO_PENDING) {
        Send(new ViewMsg_SignalCookiePromptEvent());
        return;
      }
    }
    callback->Run(policy);
  }
}

void RenderMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {

  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadRawCookies(
          render_process_id_)) {
    ViewHostMsg_GetRawCookies::WriteReplyParams(
        reply_msg,
        std::vector<webkit_glue::WebCookie>());
    Send(reply_msg);
    return;
  }

  GetCookiesCompletion* callback = new GetCookiesCompletion(
      render_process_id_, reply_msg->routing_id(), url, reply_msg, this,
      context, true);

  // We check policy here to avoid sending back cookies that would not normally
  // be applied to outbound requests for the given URL.  Since this cookie info
  // is visible in the developer tools, it is helpful to make it match reality.
  int policy = net::OK;
  if (context->cookie_policy()) {
    policy = context->cookie_policy()->CanGetCookies(
       url, first_party_for_cookies, callback);
    if (policy == net::ERR_IO_PENDING) {
      Send(new ViewMsg_SignalCookiePromptEvent());
      return;
    }
  }
  callback->Run(policy);
}

void RenderMessageFilter::OnDeleteCookie(const GURL& url,
                                         const std::string& cookie_name) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookie(url, cookie_name);
}

void RenderMessageFilter::OnCookiesEnabled(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  CookiesEnabledCompletion* callback =
      new CookiesEnabledCompletion(reply_msg, this);
  int policy = net::OK;
  // TODO(ananta): If this render view is associated with an automation channel,
  // aka ChromeFrame then we need to retrieve cookie settings from the external
  // host.
  if (context->cookie_policy()) {
    policy = context->cookie_policy()->CanGetCookies(
        url, first_party_for_cookies, callback);
    if (policy == net::ERR_IO_PENDING) {
      Send(new ViewMsg_SignalCookiePromptEvent());
      return;  // CanGetCookies will call our callback in this case.
    }
  }
  callback->Run(policy);
}

#if defined(OS_MACOSX)
void RenderMessageFilter::OnLoadFont(const FontDescriptor& font,
                                     uint32* handle_size,
                                     base::SharedMemoryHandle* handle) {
  base::SharedMemory font_data;
  uint32 font_data_size = 0;
  bool ok = FontLoader::LoadFontIntoBuffer(font.nsFont(), &font_data,
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

void RenderMessageFilter::OnGetPlugins(bool refresh,
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
      if (now - last_plugin_refresh_time_ < threshold)
        refresh = false;  // Ignore refresh request; threshold not exceeded yet.
      else
        last_plugin_refresh_time_ = now;
  }

  // Can't load plugins on IO thread, so go to the FILE thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OnGetPluginsOnFileThread, refresh,
          reply_msg));
}

void RenderMessageFilter::OnGetPluginsOnFileThread(
    bool refresh, IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::vector<webkit::npapi::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetEnabledPlugins(refresh, &plugins);
  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &RenderMessageFilter::Send, reply_msg));
}

void RenderMessageFilter::OnGetPluginInfo(int routing_id,
                                          const GURL& url,
                                          const GURL& policy_url,
                                          const std::string& mime_type,
                                          IPC::Message* reply_msg) {
  // The PluginService::GetFirstAllowedPluginInfo may need to load the
  // plugins.  Don't do it on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OnGetPluginInfoOnFileThread,
          routing_id, url, policy_url, mime_type, reply_msg));
}

void RenderMessageFilter::OnGetPluginInfoOnFileThread(
    int render_view_id,
    const GURL& url,
    const GURL& policy_url,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  std::string actual_mime_type;
  webkit::npapi::WebPluginInfo info;
  bool found = plugin_service_->GetFirstAllowedPluginInfo(
      render_process_id_, render_view_id, url, mime_type, &info,
      &actual_mime_type);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OnGotPluginInfo,
          found, info, actual_mime_type, policy_url, reply_msg));
}

void RenderMessageFilter::OnGotPluginInfo(
    bool found,
    const webkit::npapi::WebPluginInfo& info,
    const std::string& actual_mime_type,
    const GURL& policy_url,
    IPC::Message* reply_msg) {
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  webkit::npapi::WebPluginInfo info_copy = info;
  if (found) {
    info_copy.enabled = info_copy.enabled &&
        plugin_service_->PrivatePluginAllowedForURL(info_copy.path, policy_url);
    std::string resource =
        webkit::npapi::PluginList::Singleton()->GetPluginGroupIdentifier(
            info_copy);
    setting = content_settings_->GetContentSetting(
        policy_url,
        CONTENT_SETTINGS_TYPE_PLUGINS,
        resource);
  }

  ViewHostMsg_GetPluginInfo::WriteReplyParams(
      reply_msg, found, info_copy, setting, actual_mime_type);
  Send(reply_msg);
}

void RenderMessageFilter::OnOpenChannelToPlugin(int routing_id,
                                                const GURL& url,
                                                const std::string& mime_type,
                                                IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPlugin(
      render_process_id_, routing_id, url, mime_type,
      new OpenChannelToPluginCallback(this, reply_msg));
}

void RenderMessageFilter::OnOpenChannelToPepperPlugin(
    const FilePath& path,
    IPC::Message* reply_msg) {
  PpapiPluginProcessHost* host = new PpapiPluginProcessHost(this);
  host->Init(path, reply_msg);
  ppapi_plugin_hosts_.push_back(linked_ptr<PpapiPluginProcessHost>(host));
}

void RenderMessageFilter::OnLaunchNaCl(
    const std::wstring& url, int channel_descriptor, IPC::Message* reply_msg) {
  NaClProcessHost* host = new NaClProcessHost(resource_dispatcher_host_, url);
  host->Launch(this, channel_descriptor, reply_msg);
}

void RenderMessageFilter::OnGenerateRoutingID(int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
}

void RenderMessageFilter::OnDownloadUrl(const IPC::Message& message,
                                        const GURL& url,
                                        const GURL& referrer) {
  net::URLRequestContext* context = request_context_->GetURLRequestContext();

  // Don't show "Save As" UI.
  bool prompt_for_save_location = false;
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           DownloadSaveInfo(),
                                           prompt_for_save_location,
                                           render_process_id_,
                                           message.routing_id(),
                                           context);
}

void RenderMessageFilter::OnClipboardWriteObjectsSync(
    const ui::Clipboard::ObjectMap& objects,
    base::SharedMemoryHandle bitmap_handle) {
  DCHECK(base::SharedMemory::IsHandleValid(bitmap_handle))
      << "Bad bitmap handle";
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  ui::Clipboard::ObjectMap* long_living_objects =
      new ui::Clipboard::ObjectMap(objects);

  // Splice the shared memory handle into the clipboard data.
  ui::Clipboard::ReplaceSharedMemHandle(long_living_objects, bitmap_handle,
                                        peer_handle());

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

void RenderMessageFilter::OnClipboardWriteObjectsAsync(
    const ui::Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and post a task to preform
  // the write on the UI thread.
  ui::Clipboard::ObjectMap* long_living_objects =
      new ui::Clipboard::ObjectMap(objects);

  // This async message doesn't support shared-memory based bitmaps; they must
  // be removed otherwise we might dereference a rubbish pointer.
  long_living_objects->erase(ui::Clipboard::CBF_SMBITMAP);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

#if !defined(USE_X11)
// On non-X11 platforms, clipboard actions can be performed on the IO thread.
// On X11, since the clipboard is linked with GTK, we either have to do this
// with GTK on the UI thread, or with Xlib on the BACKGROUND_X11 thread. In an
// ideal world, we would do the latter. However, for now we're going to
// terminate these calls on the UI thread. This risks deadlock in the case of
// plugins, but it's better than crashing which is what doing on the IO thread
// gives us.
//
// See resource_message_filter_gtk.cc for the Linux implementation of these
// functions.

void RenderMessageFilter::OnClipboardIsFormatAvailable(
    ui::Clipboard::FormatType format, ui::Clipboard::Buffer buffer,
    IPC::Message* reply) {
  const bool result = GetClipboard()->IsFormatAvailable(format, buffer);
  ViewHostMsg_ClipboardIsFormatAvailable::WriteReplyParams(reply, result);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadText(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply) {
  string16 result;
  GetClipboard()->ReadText(buffer, &result);
  ViewHostMsg_ClipboardReadText::WriteReplyParams(reply, result);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadAsciiText(ui::Clipboard::Buffer buffer,
                                                   IPC::Message* reply) {
  std::string result;
  GetClipboard()->ReadAsciiText(buffer, &result);
  ViewHostMsg_ClipboardReadAsciiText::WriteReplyParams(reply, result);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                                              IPC::Message* reply) {
  std::string src_url_str;
  string16 markup;
  GetClipboard()->ReadHTML(buffer, &markup, &src_url_str);
  const GURL src_url = GURL(src_url_str);

  ViewHostMsg_ClipboardReadHTML::WriteReplyParams(reply, markup, src_url);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadAvailableTypes(
    ui::Clipboard::Buffer buffer, IPC::Message* reply) {
  std::vector<string16> types;
  bool contains_filenames = false;
  bool result = ClipboardDispatcher::ReadAvailableTypes(
      buffer, &types, &contains_filenames);
  ViewHostMsg_ClipboardReadAvailableTypes::WriteReplyParams(
      reply, result, types, contains_filenames);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadData(
    ui::Clipboard::Buffer buffer, const string16& type, IPC::Message* reply) {
  string16 data;
  string16 metadata;
  bool result = ClipboardDispatcher::ReadData(buffer, type, &data, &metadata);
  ViewHostMsg_ClipboardReadData::WriteReplyParams(
      reply, result, data, metadata);
  Send(reply);
}

void RenderMessageFilter::OnClipboardReadFilenames(
    ui::Clipboard::Buffer buffer, IPC::Message* reply) {
  std::vector<string16> filenames;
  bool result = ClipboardDispatcher::ReadFilenames(buffer, &filenames);
  ViewHostMsg_ClipboardReadFilenames::WriteReplyParams(
      reply, result, filenames);
  Send(reply);
}

#endif

void RenderMessageFilter::OnCheckNotificationPermission(
    const GURL& source_url, int* result) {
  *result = WebKit::WebNotificationPresenter::PermissionNotAllowed;

  ChromeURLRequestContext* context = GetRequestContextForURL(source_url);
  if (context->extension_info_map()->CheckURLAccessToExtensionPermission(
          source_url, Extension::kNotificationPermission)) {
    *result = WebKit::WebNotificationPresenter::PermissionAllowed;
    return;
  }

  // Fall back to the regular notification preferences, which works on an
  // origin basis.
  *result = notification_prefs_->HasPermission(source_url.GetOrigin());
}

void RenderMessageFilter::OnGetCPBrowsingContext(uint32* context) {
  // Always allocate a new context when a plugin requests one, since it needs to
  // be unique for that plugin instance.
  *context = CPBrowsingContextManager::GetInstance()->Allocate(
      request_context_->GetURLRequestContext());
}

#if defined(OS_WIN)
void RenderMessageFilter::OnDuplicateSection(
    base::SharedMemoryHandle renderer_handle,
    base::SharedMemoryHandle* browser_handle) {
  // Duplicate the handle in this process right now so the memory is kept alive
  // (even if it is not mapped)
  base::SharedMemory shared_buf(renderer_handle, true, peer_handle());
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), browser_handle);
}
#endif

#if defined(OS_POSIX)
void RenderMessageFilter::OnAllocateSharedMemoryBuffer(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  if (!shared_buf.CreateAndMapAnonymous(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
    return;
  }
  shared_buf.GiveToProcess(base::GetCurrentProcessHandle(), handle);
}
#endif

void RenderMessageFilter::OnResourceTypeStats(
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
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(
          &RenderMessageFilter::OnResourceTypeStatsOnUIThread,
          stats,
          base::GetProcId(peer_handle())));
}

void RenderMessageFilter::OnResourceTypeStatsOnUIThread(
    const WebCache::ResourceTypeStats& stats, base::ProcessId renderer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TaskManager::GetInstance()->model()->NotifyResourceTypeStats(
      renderer_id, stats);
}


void RenderMessageFilter::OnV8HeapStats(int v8_memory_allocated,
                                        int v8_memory_used) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&RenderMessageFilter::OnV8HeapStatsOnUIThread,
                          v8_memory_allocated,
                          v8_memory_used,
                          base::GetProcId(peer_handle())));
}

// static
void RenderMessageFilter::OnV8HeapStatsOnUIThread(
    int v8_memory_allocated, int v8_memory_used, base::ProcessId renderer_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TaskManager::GetInstance()->model()->NotifyV8HeapStats(
      renderer_id,
      static_cast<size_t>(v8_memory_allocated),
      static_cast<size_t>(v8_memory_used));
}

void RenderMessageFilter::OnDidZoomURL(const IPC::Message& message,
                                       double zoom_level,
                                       bool remember,
                                       const GURL& url) {
  Task* task = NewRunnableMethod(this,
      &RenderMessageFilter::UpdateHostZoomLevelsOnUIThread, zoom_level,
      remember, url, render_process_id_, message.routing_id());
#if defined(OS_MACOSX)
  cocoa_utils::PostTaskInEventTrackingRunLoopMode(FROM_HERE, task);
#else
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, task);
#endif
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

void RenderMessageFilter::OnResolveProxy(const GURL& url,
                                         IPC::Message* reply_msg) {
  resolve_proxy_msg_helper_.Start(url, reply_msg);
}

void RenderMessageFilter::OnResolveProxyCompleted(
    IPC::Message* reply_msg,
    int result,
    const std::string& proxy_list) {
  ViewHostMsg_ResolveProxy::WriteReplyParams(reply_msg, result, proxy_list);
  Send(reply_msg);
}

void RenderMessageFilter::OnGetDefaultPrintSettings(IPC::Message* reply_msg) {
  scoped_refptr<printing::PrinterQuery> printer_query;
  if (!print_job_manager_->printing_enabled()) {
    // Reply with NULL query.
    OnGetDefaultPrintSettingsReply(printer_query, reply_msg);
    return;
  }

  print_job_manager_->PopPrinterQuery(0, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &RenderMessageFilter::OnGetDefaultPrintSettingsReply,
      printer_query,
      reply_msg);
  // Loads default settings. This is asynchronous, only the IPC message sender
  // will hang until the settings are retrieved.
  printer_query->GetSettings(printing::PrinterQuery::DEFAULTS,
                             NULL,
                             0,
                             false,
                             true,
                             task);
}

void RenderMessageFilter::OnGetDefaultPrintSettingsReply(
    scoped_refptr<printing::PrinterQuery> printer_query,
    IPC::Message* reply_msg) {
  ViewMsg_Print_Params params;
  if (!printer_query.get() ||
      printer_query->last_status() != printing::PrintingContext::OK) {
    memset(&params, 0, sizeof(params));
  } else {
    RenderParamsFromPrintSettings(printer_query->settings(), &params);
    params.document_cookie = printer_query->cookie();
  }
  ViewHostMsg_GetDefaultPrintSettings::WriteReplyParams(reply_msg, params);
  Send(reply_msg);
  // If printing was enabled.
  if (printer_query.get()) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      print_job_manager_->QueuePrinterQuery(printer_query.get());
    } else {
      printer_query->StopWorker();
    }
  }
}

void RenderMessageFilter::OnScriptedPrint(
    const ViewHostMsg_ScriptedPrint_Params& params,
    IPC::Message* reply_msg) {
  gfx::NativeView host_view =
      gfx::NativeViewFromIdInBrowser(params.host_window_id);

  scoped_refptr<printing::PrinterQuery> printer_query;
  print_job_manager_->PopPrinterQuery(params.cookie, &printer_query);
  if (!printer_query.get()) {
    printer_query = new printing::PrinterQuery;
  }

  CancelableTask* task = NewRunnableMethod(
      this,
      &RenderMessageFilter::OnScriptedPrintReply,
      printer_query,
      params.routing_id,
      reply_msg);

  printer_query->GetSettings(printing::PrinterQuery::ASK_USER,
                             host_view,
                             params.expected_pages_count,
                             params.has_selection,
                             params.use_overlays,
                             task);
}

void RenderMessageFilter::OnScriptedPrintReply(
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

// static
ui::Clipboard* RenderMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ui::Clipboard* clipboard = new ui::Clipboard;

  return clipboard;
}

ChromeURLRequestContext* RenderMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  URLRequestContextGetter* context_getter =
      url.SchemeIs(chrome::kExtensionScheme) ?
          extensions_request_context_ : request_context_;
  return static_cast<ChromeURLRequestContext*>(
      context_getter->GetURLRequestContext());
}

void RenderMessageFilter::OnPlatformCheckSpelling(const string16& word,
                                                  int tag,
                                                  bool* correct) {
  *correct = SpellCheckerPlatform::CheckSpelling(word, tag);
}

void RenderMessageFilter::OnPlatformFillSuggestionList(
    const string16& word,
    std::vector<string16>* suggestions) {
  SpellCheckerPlatform::FillSuggestionList(word, suggestions);
}

void RenderMessageFilter::OnGetDocumentTag(IPC::Message* reply_msg) {
  int tag = SpellCheckerPlatform::GetDocumentTag();
  ViewHostMsg_GetDocumentTag::WriteReplyParams(reply_msg, tag);
  Send(reply_msg);
  return;
}

void RenderMessageFilter::OnDocumentWithTagClosed(int tag) {
  SpellCheckerPlatform::CloseDocumentWithTag(tag);
}

void RenderMessageFilter::OnShowSpellingPanel(bool show) {
  SpellCheckerPlatform::ShowSpellingPanel(show);
}

void RenderMessageFilter::OnUpdateSpellingPanelWithMisspelledWord(
    const string16& word) {
  SpellCheckerPlatform::UpdateSpellingPanelWithMisspelledWord(word);
}

void RenderMessageFilter::OnDnsPrefetch(
    const std::vector<std::string>& hostnames) {
  chrome_browser_net::DnsPrefetchList(hostnames);
}

void RenderMessageFilter::OnRendererHistograms(
    int sequence_number,
    const std::vector<std::string>& histograms) {
  HistogramSynchronizer::DeserializeHistogramList(sequence_number, histograms);
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

void RenderMessageFilter::OnOpenChannelToExtension(
    int routing_id, const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OpenChannelToExtensionOnUIThread,
          render_process_id_, routing_id, port2_id, source_extension_id,
          target_extension_id, channel_name));
}

void RenderMessageFilter::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_->GetExtensionMessageService()->OpenChannelToExtension(
      source_process_id, source_routing_id, receiver_port_id,
      source_extension_id, target_extension_id, channel_name);
}

void RenderMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OpenChannelToTabOnUIThread,
          render_process_id_, routing_id, port2_id, tab_id, extension_id,
          channel_name));
}

void RenderMessageFilter::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    int tab_id,
    const std::string& extension_id,
    const std::string& channel_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_->GetExtensionMessageService()->OpenChannelToTab(
      source_process_id, source_routing_id, receiver_port_id,
      tab_id, extension_id, channel_name);
}

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
      http_transaction_factory()->GetCache()->CloseCurrentConnections();
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

void RenderMessageFilter::OnClearCache(IPC::Message* reply_msg) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  int rv = -1;
  if (CheckBenchmarkingEnabled()) {
    disk_cache::Backend* backend = request_context_->GetURLRequestContext()->
        http_transaction_factory()->GetCache()->GetCurrentBackend();
    if (backend) {
      ClearCacheCompletion* callback =
          new ClearCacheCompletion(reply_msg, this);
      rv = backend->DoomAllEntries(callback);
      if (rv == net::ERR_IO_PENDING) {
        // The callback will send the reply.
        return;
      }
      // Completed synchronously, no need for the callback.
      delete callback;
    }
  }
  ViewHostMsg_ClearCache::WriteReplyParams(reply_msg, rv);
  Send(reply_msg);
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

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &RenderMessageFilter::Send, reply_msg));
}

#if defined(USE_TCMALLOC)
void RenderMessageFilter::OnRendererTcmalloc(base::ProcessId pid,
                                             const std::string& output) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(AboutTcmallocRendererCallback, pid, output));
}
#endif

void RenderMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
    request_context_->GetURLRequestContext());

  FilePath extension_path =
      context->extension_info_map()->GetPathForExtension(extension_id);
  std::string default_locale =
      context->extension_info_map()->GetDefaultLocaleForExtension(extension_id);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &RenderMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          extension_path, extension_id, default_locale, reply_msg));
}

void RenderMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::map<std::string, std::string> dictionary_map;
  if (!default_locale.empty()) {
    // Touch disk only if extension is localized.
    std::string error;
    scoped_ptr<ExtensionMessageBundle> bundle(
        extension_file_util::LoadExtensionMessageBundle(
            extension_path, default_locale, &error));

    if (bundle.get())
      dictionary_map = *bundle->dictionary();
  }

  // Add @@extension_id reserved message here, so it's available to
  // non-localized extensions too.
  dictionary_map.insert(
      std::make_pair(ExtensionMessageBundle::kExtensionIdKey, extension_id));

  ViewHostMsg_GetExtensionMessageBundle::WriteReplyParams(
      reply_msg, dictionary_map);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &RenderMessageFilter::Send, reply_msg));
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

SetCookieCompletion::SetCookieCompletion(int render_process_id,
                                         int render_view_id,
                                         const GURL& url,
                                         const std::string& cookie_line,
                                         ChromeURLRequestContext* context)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      url_(url),
      cookie_line_(cookie_line),
      context_(context) {
}

SetCookieCompletion::~SetCookieCompletion() {}

void SetCookieCompletion::RunWithParams(const Tuple1<int>& params) {
  int result = params.a;
  bool blocked_by_policy = true;
  net::CookieOptions options;
  if (result == net::OK ||
      result == net::OK_FOR_SESSION_ONLY) {
    blocked_by_policy = false;
    if (result == net::OK_FOR_SESSION_ONLY)
      options.set_force_session();
    context_->cookie_store()->SetCookieWithOptions(url_, cookie_line_,
                                                   options);
  }
  CallRenderViewHostContentSettingsDelegate(
      render_process_id_, render_view_id_,
      &RenderViewHostDelegate::ContentSettings::OnCookieChanged,
      url_, cookie_line_, options, blocked_by_policy);
  delete this;
}

GetCookiesCompletion::GetCookiesCompletion(int render_process_id,
                                           int render_view_id,
                                           const GURL& url,
                                           IPC::Message* reply_msg,
                                           RenderMessageFilter* filter,
                                           ChromeURLRequestContext* context,
                                           bool raw_cookies)
    : url_(url),
      reply_msg_(reply_msg),
      filter_(filter),
      context_(context),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      raw_cookies_(raw_cookies) {
  set_cookie_store(context_->cookie_store());
}

GetCookiesCompletion::~GetCookiesCompletion() {}

void GetCookiesCompletion::RunWithParams(const Tuple1<int>& params) {
  if (!raw_cookies_) {
    int result = params.a;
    std::string cookies;
    if (result == net::OK)
      cookies = cookie_store()->GetCookies(url_);
    ViewHostMsg_GetCookies::WriteReplyParams(reply_msg_, cookies);
    filter_->Send(reply_msg_);
    net::CookieMonster* cookie_monster =
        context_->cookie_store()->GetCookieMonster();
    net::CookieList cookie_list =
        cookie_monster->GetAllCookiesForURLWithOptions(
            url_, net::CookieOptions());
    CallRenderViewHostContentSettingsDelegate(
        render_process_id_, render_view_id_,
        &RenderViewHostDelegate::ContentSettings::OnCookiesRead,
        url_, cookie_list, result != net::OK);
    delete this;
  } else {
    // Ignore the policy result.  We only waited on the policy result so that
    // any pending 'set-cookie' requests could be flushed.  The intent of
    // querying the raw cookies is to reveal the contents of the cookie DB, so
    // it important that we don't read the cookie db ahead of pending writes.
    net::CookieMonster* cookie_monster =
        context_->cookie_store()->GetCookieMonster();
    net::CookieList cookie_list = cookie_monster->GetAllCookiesForURL(url_);

    std::vector<webkit_glue::WebCookie> cookies;
    for (size_t i = 0; i < cookie_list.size(); ++i) {
      cookies.push_back(webkit_glue::WebCookie(cookie_list[i]));
    }

    ViewHostMsg_GetRawCookies::WriteReplyParams(reply_msg_, cookies);
    filter_->Send(reply_msg_);
    delete this;
  }
}

void GetCookiesCompletion::set_cookie_store(CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
}

CookiesEnabledCompletion::CookiesEnabledCompletion(
    IPC::Message* reply_msg,
    RenderMessageFilter* filter)
    : reply_msg_(reply_msg),
      filter_(filter) {
}

CookiesEnabledCompletion::~CookiesEnabledCompletion() {}

void CookiesEnabledCompletion::RunWithParams(const Tuple1<int>& params) {
  bool result = params.a != net::ERR_ACCESS_DENIED;
  ViewHostMsg_CookiesEnabled::WriteReplyParams(reply_msg_, result);
  filter_->Send(reply_msg_);
  delete this;
}
