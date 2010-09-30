// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/resource_message_filter.h"

#include "app/clipboard/clipboard.h"
#include "base/callback.h"
#include "base/command_line.h"
#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif
#include "base/file_util.h"
#include "base/file_path.h"
#include "base/histogram.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/thread.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/worker_pool.h"
#include "chrome/browser/appcache/appcache_dispatcher_host.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/clipboard_dispatcher.h"
#include "chrome/browser/device_orientation/dispatcher_host.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/file_system/file_system_dispatcher_host.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/geolocation/geolocation_dispatcher_host.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/in_process_webkit/indexed_db_dispatcher_host.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/audio_renderer_host.h"
#include "chrome/browser/renderer_host/blob_dispatcher_host.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/database_dispatcher_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/render_widget_helper.h"
#include "chrome/browser/search_engines/search_provider_install_state_dispatcher_host.h"
#include "chrome/browser/speech/speech_input_dispatcher_host.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui_thread_helpers.h"
#include "chrome/browser/worker_host/message_port_dispatcher.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/child_process_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#if defined(OS_MACOSX)
#include "chrome/common/font_descriptor_mac.h"
#include "chrome/common/font_loader_mac.h"
#endif
#include "chrome/common/notification_service.h"
#include "chrome/common/plugin_group.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/worker_messages.h"
#include "gfx/native_widget_types.h"
#include "net/base/cookie_monster.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPresenter.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webkit_glue.h"

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
}

class ClearCacheCompletion : public net::CompletionCallback {
 public:
  ClearCacheCompletion(IPC::Message* reply_msg,
                       ResourceMessageFilter* filter)
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
  scoped_refptr<ResourceMessageFilter> filter_;
};

void WriteFileSize(IPC::Message* reply_msg,
                   const base::PlatformFileInfo& file_info) {
  ViewHostMsg_GetFileSize::WriteReplyParams(reply_msg, file_info.size);
}

void WriteFileModificationTime(IPC::Message* reply_msg,
                               const base::PlatformFileInfo& file_info) {
  ViewHostMsg_GetFileModificationTime::WriteReplyParams(
      reply_msg, file_info.last_modified);
}

}  // namespace

ResourceMessageFilter::ResourceMessageFilter(
    ResourceDispatcherHost* resource_dispatcher_host,
    int child_id,
    AudioRendererHost* audio_renderer_host,
    PluginService* plugin_service,
    printing::PrintJobManager* print_job_manager,
    Profile* profile,
    RenderWidgetHelper* render_widget_helper)
    : Receiver(RENDER_PROCESS, child_id),
      channel_(NULL),
      resource_dispatcher_host_(resource_dispatcher_host),
      plugin_service_(plugin_service),
      print_job_manager_(print_job_manager),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(resolve_proxy_msg_helper_(this, NULL)),
      media_request_context_(profile->GetRequestContextForMedia()),
      extensions_request_context_(profile->GetRequestContextForExtensions()),
      render_widget_helper_(render_widget_helper),
      audio_renderer_host_(audio_renderer_host),
      appcache_dispatcher_host_(
          new AppCacheDispatcherHost(profile->GetRequestContext())),
      ALLOW_THIS_IN_INITIALIZER_LIST(dom_storage_dispatcher_host_(
          new DOMStorageDispatcherHost(this, profile->GetWebKitContext()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(indexed_db_dispatcher_host_(
          new IndexedDBDispatcherHost(this, profile))),
      ALLOW_THIS_IN_INITIALIZER_LIST(db_dispatcher_host_(
          new DatabaseDispatcherHost(profile->GetDatabaseTracker(), this,
              profile->GetHostContentSettingsMap()))),
      notification_prefs_(
          profile->GetDesktopNotificationService()->prefs_cache()),
      host_zoom_map_(profile->GetHostZoomMap()),
      off_the_record_(profile->IsOffTheRecord()),
      next_route_id_callback_(NewCallbackWithReturnValue(
          render_widget_helper, &RenderWidgetHelper::GetNextRoutingID)),
      ALLOW_THIS_IN_INITIALIZER_LIST(speech_input_dispatcher_host_(
          new speech_input::SpeechInputDispatcherHost(this->id()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(geolocation_dispatcher_host_(
          GeolocationDispatcherHost::New(
              this->id(), profile->GetGeolocationPermissionContext()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          search_provider_install_state_dispatcher_host_(
              new SearchProviderInstallStateDispatcherHost(this, profile,
                                                           child_id))),
      ALLOW_THIS_IN_INITIALIZER_LIST(device_orientation_dispatcher_host_(
          new device_orientation::DispatcherHost(this->id()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(file_system_dispatcher_host_(
          new FileSystemDispatcherHost(this,
              profile->GetFileSystemHostContext(),
              profile->GetHostContentSettingsMap()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(blob_dispatcher_host_(
          new BlobDispatcherHost(
              this->id(), profile->GetBlobStorageContext()))) {
  request_context_ = profile_->GetRequestContext();
  DCHECK(request_context_);
  DCHECK(media_request_context_);
  DCHECK(audio_renderer_host_.get());
  DCHECK(appcache_dispatcher_host_.get());
  DCHECK(dom_storage_dispatcher_host_.get());

  render_widget_helper_->Init(id(), resource_dispatcher_host_);
#if defined(OS_CHROMEOS)
  cloud_print_enabled_ = true;
#else
  cloud_print_enabled_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrint);
#endif
}

ResourceMessageFilter::~ResourceMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // Tell the DOM Storage dispatcher host to stop sending messages via us.
  dom_storage_dispatcher_host_->Shutdown();

  // Tell the Indexed DB dispatcher host to stop sending messages via us.
  indexed_db_dispatcher_host_->Shutdown();

  // Shut down the database dispatcher host.
  db_dispatcher_host_->Shutdown();

  // Shut down the async file_system dispatcher host.
  file_system_dispatcher_host_->Shutdown();

  // Shut down the blob dispatcher host.
  blob_dispatcher_host_->Shutdown();

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
  appcache_dispatcher_host_->Initialize(this);
  dom_storage_dispatcher_host_->Init(id(), handle());
  indexed_db_dispatcher_host_->Init(id(), handle());
  db_dispatcher_host_->Init(handle());
  file_system_dispatcher_host_->Init(handle());
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
      indexed_db_dispatcher_host_->OnMessageReceived(msg) ||
      audio_renderer_host_->OnMessageReceived(msg, &msg_is_ok) ||
      db_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      mp_dispatcher->OnMessageReceived(
          msg, this, next_route_id_callback(), &msg_is_ok) ||
      geolocation_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      speech_input_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      search_provider_install_state_dispatcher_host_->OnMessageReceived(
          msg, &msg_is_ok) ||
      device_orientation_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      file_system_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok) ||
      blob_dispatcher_host_->OnMessageReceived(msg, &msg_is_ok);

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
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateFullscreenWidget,
                          OnMsgCreateFullscreenWidget)
      IPC_MESSAGE_HANDLER(ViewHostMsg_SetCookie, OnSetCookie)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetCookies, OnGetCookies)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetRawCookies,
                                      OnGetRawCookies)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DeleteCookie, OnDeleteCookie)
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_LoadFont, OnLoadFont)
#endif
#if defined(OS_WIN)  // This hack is Windows-specific.
      IPC_MESSAGE_HANDLER(ViewHostMsg_PreCacheFont, OnPreCacheFont)
#endif
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetPlugins, OnGetPlugins)
      IPC_MESSAGE_HANDLER(ViewHostMsg_GetPluginInfo, OnGetPluginInfo)
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
#if defined(OS_MACOSX)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AllocatePDFTransport,
                          OnAllocateSharedMemoryBuffer)
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
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ScriptedPrint,
                                      OnScriptedPrint)
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
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_ClearCache, OnClearCache)
      IPC_MESSAGE_HANDLER(ViewHostMsg_DidGenerateCacheableMetadata,
                          OnCacheableMetadataAvailable)
      IPC_MESSAGE_HANDLER(ViewHostMsg_EnableSpdy, OnEnableSpdy)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileSize, OnGetFileSize)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetFileModificationTime,
                                      OnGetFileModificationTime)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_OpenFile, OnOpenFile)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_Keygen, OnKeygen)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_GetExtensionMessageBundle,
                                      OnGetExtensionMessageBundle)
#if defined(USE_TCMALLOC)
      IPC_MESSAGE_HANDLER(ViewHostMsg_RendererTcmalloc, OnRendererTcmalloc)
#endif
      IPC_MESSAGE_HANDLER(ViewHostMsg_EstablishGpuChannel,
                          OnEstablishGpuChannel)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SynchronizeGpu,
                                      OnSynchronizeGpu)
      IPC_MESSAGE_HANDLER(ViewHostMsg_AsyncOpenFile, OnAsyncOpenFile)
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
    const ViewHostMsg_CreateWindow_Params& params,
    int* route_id, int64* cloned_session_storage_namespace_id) {
  *cloned_session_storage_namespace_id = dom_storage_dispatcher_host_->
      CloneSessionStorage(params.session_storage_namespace_id);
  render_widget_helper_->CreateNewWindow(params.opener_id,
                                         params.user_gesture,
                                         params.window_container_type,
                                         params.frame_name,
                                         handle(),
                                         route_id);
}

void ResourceMessageFilter::OnMsgCreateWidget(int opener_id,
                                              WebKit::WebPopupType popup_type,
                                              int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, popup_type, route_id);
}

void ResourceMessageFilter::OnMsgCreateFullscreenWidget(
    int opener_id, WebKit::WebPopupType popup_type, int* route_id) {
  render_widget_helper_->CreateNewFullscreenWidget(
      opener_id, popup_type, route_id);
}

void ResourceMessageFilter::OnSetCookie(const IPC::Message& message,
                                        const GURL& url,
                                        const GURL& first_party_for_cookies,
                                        const std::string& cookie) {
  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  SetCookieCompletion* callback =
      new SetCookieCompletion(id(), message.routing_id(), url, cookie,
                              context);

  // If this render view is associated with an automation channel, aka
  // ChromeFrame then we need to set cookies in the external host.
  if (!AutomationResourceMessageFilter::SetCookiesForUrl(url, cookie,
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

void ResourceMessageFilter::OnGetCookies(const GURL& url,
                                         const GURL& first_party_for_cookies,
                                         IPC::Message* reply_msg) {
  URLRequestContext* context = GetRequestContextForURL(url);

  GetCookiesCompletion* callback =
      new GetCookiesCompletion(id(), reply_msg->routing_id(), url, reply_msg,
                               this, context, false);

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

void ResourceMessageFilter::OnGetRawCookies(
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg) {

  ChromeURLRequestContext* context = GetRequestContextForURL(url);

  // Only return raw cookies to trusted renderers or if this request is
  // not targeted to an an external host like ChromeFrame.
  // TODO(ananta) We need to support retreiving raw cookies from external
  // hosts.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadRawCookies(id()) ||
      context->IsExternal()) {
    ViewHostMsg_GetRawCookies::WriteReplyParams(
        reply_msg,
        std::vector<webkit_glue::WebCookie>());
    Send(reply_msg);
    return;
  }

  GetCookiesCompletion* callback =
      new GetCookiesCompletion(id(), reply_msg->routing_id(), url,
                               reply_msg, this, context, true);

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

void ResourceMessageFilter::OnDeleteCookie(const GURL& url,
                                           const std::string& cookie_name) {
  URLRequestContext* context = GetRequestContextForURL(url);
  context->cookie_store()->DeleteCookie(url, cookie_name);
}

#if defined(OS_MACOSX)
void ResourceMessageFilter::OnLoadFont(const FontDescriptor& font,
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
void ResourceMessageFilter::OnPreCacheFont(LOGFONT font) {
  ChildProcessHost::PreCacheFont(font);
}
#endif  // OS_WIN

void ResourceMessageFilter::OnGetPlugins(bool refresh,
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
  NPAPI::PluginList::Singleton()->GetEnabledPlugins(refresh, &plugins);
  ViewHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnGetPluginInfo(const GURL& url,
                                            const GURL& policy_url,
                                            const std::string& mime_type,
                                            bool* found,
                                            WebPluginInfo* info,
                                            ContentSetting* setting,
                                            std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = NPAPI::PluginList::Singleton()->GetPluginInfo(url,
                                                         mime_type,
                                                         allow_wildcard,
                                                         info,
                                                         actual_mime_type);
  if (*found) {
    info->enabled = info->enabled &&
        plugin_service_->PrivatePluginAllowedForURL(info->path, policy_url);
    HostContentSettingsMap* map = profile_->GetHostContentSettingsMap();
    scoped_ptr<PluginGroup> group(PluginGroup::CopyOrCreatePluginGroup(*info));
    std::string resource = group->identifier();
    *setting = map->GetNonDefaultContentSetting(
        policy_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource);
    if (*setting == CONTENT_SETTING_DEFAULT) {
      ContentSetting defaultContentSetting =
          map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS);
      if (defaultContentSetting == CONTENT_SETTING_BLOCK ||
          !map->GetBlockNonsandboxedPlugins()) {
        *setting = defaultContentSetting;
      }
    }
  }
}

void ResourceMessageFilter::OnOpenChannelToPlugin(const GURL& url,
                                                  const std::string& mime_type,
                                                  const std::string& locale,
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
  *route_id = params.route_id != MSG_ROUTING_NONE ?
      params.route_id : render_widget_helper_->GetNextRoutingID();
  if (params.is_shared)
    WorkerService::GetInstance()->CreateSharedWorker(
        params.url, off_the_record(), params.name,
        params.document_id, id(), params.render_view_route_id, this, *route_id,
        params.script_resource_appcache_id,
        static_cast<ChromeURLRequestContext*>(
            request_context_->GetURLRequestContext()));
  else
    WorkerService::GetInstance()->CreateDedicatedWorker(
        params.url, off_the_record(),
        params.document_id, id(), params.render_view_route_id, this, *route_id,
        id(), params.parent_appcache_host_id,
        static_cast<ChromeURLRequestContext*>(
            request_context_->GetURLRequestContext()));
}

void ResourceMessageFilter::OnLookupSharedWorker(
    const ViewHostMsg_CreateWorker_Params& params, bool* exists, int* route_id,
    bool* url_mismatch) {
  *route_id = render_widget_helper_->GetNextRoutingID();
  *exists = WorkerService::GetInstance()->LookupSharedWorker(
      params.url, params.name, off_the_record(), params.document_id, id(),
      params.render_view_route_id, this, *route_id, url_mismatch);
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

  // Don't show "Save As" UI.
  bool prompt_for_save_location = false;
  resource_dispatcher_host_->BeginDownload(url,
                                           referrer,
                                           DownloadSaveInfo(),
                                           prompt_for_save_location,
                                           id(),
                                           message.routing_id(),
                                           context);
}

void ResourceMessageFilter::OnClipboardWriteObjectsSync(
    const Clipboard::ObjectMap& objects,
    base::SharedMemoryHandle bitmap_handle) {
  DCHECK(base::SharedMemory::IsHandleValid(bitmap_handle))
      << "Bad bitmap handle";
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and get a handle to any
  // shared memory so it doesn't go away when we resume the renderer, and post
  // a task to perform the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

  // Splice the shared memory handle into the clipboard data.
  Clipboard::ReplaceSharedMemHandle(long_living_objects, bitmap_handle,
                                    handle());

  ChromeThread::PostTask(
      ChromeThread::UI,
      FROM_HERE,
      new WriteClipboardTask(long_living_objects));
}

void ResourceMessageFilter::OnClipboardWriteObjectsAsync(
    const Clipboard::ObjectMap& objects) {
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and post a task to preform
  // the write on the UI thread.
  Clipboard::ObjectMap* long_living_objects = new Clipboard::ObjectMap(objects);

  // This async message doesn't support shared-memory based bitmaps; they must
  // be removed otherwise we might dereference a rubbish pointer.
  long_living_objects->erase(Clipboard::CBF_SMBITMAP);

  ChromeThread::PostTask(
      ChromeThread::UI,
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

void ResourceMessageFilter::OnClipboardReadAvailableTypes(
    Clipboard::Buffer buffer, IPC::Message* reply) {
  std::vector<string16> types;
  bool contains_filenames = false;
  bool result = ClipboardDispatcher::ReadAvailableTypes(
      buffer, &types, &contains_filenames);
  ViewHostMsg_ClipboardReadAvailableTypes::WriteReplyParams(
      reply, result, types, contains_filenames);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadData(
    Clipboard::Buffer buffer, const string16& type, IPC::Message* reply) {
  string16 data;
  string16 metadata;
  bool result = ClipboardDispatcher::ReadData(buffer, type, &data, &metadata);
  ViewHostMsg_ClipboardReadData::WriteReplyParams(
      reply, result, data, metadata);
  Send(reply);
}

void ResourceMessageFilter::OnClipboardReadFilenames(
    Clipboard::Buffer buffer, IPC::Message* reply) {
  std::vector<string16> filenames;
  bool result = ClipboardDispatcher::ReadFilenames(buffer, &filenames);
  ViewHostMsg_ClipboardReadFilenames::WriteReplyParams(
      reply, result, filenames);
  Send(reply);
}

#endif

void ResourceMessageFilter::OnCheckNotificationPermission(
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

#if defined(OS_POSIX)
void ResourceMessageFilter::OnAllocateSharedMemoryBuffer(
    uint32 buffer_size,
    base::SharedMemoryHandle* handle) {
  base::SharedMemory shared_buf;
  shared_buf.Create("", false, false, buffer_size);
  if (!shared_buf.Map(buffer_size)) {
    *handle = base::SharedMemory::NULLHandle();
    NOTREACHED() << "Cannot map shared memory buffer";
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

void ResourceMessageFilter::OnDidZoomURL(const GURL& url,
                                         int zoom_level) {
  ui_thread_helpers::PostTaskWhileRunningMenu(FROM_HERE,
      NewRunnableMethod(this,
                        &ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread,
                        url, zoom_level));
}

void ResourceMessageFilter::UpdateHostZoomLevelsOnUIThread(
    const GURL& url,
    int zoom_level) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
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
                             true,
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

void ResourceMessageFilter::OnScriptedPrint(
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
      &ResourceMessageFilter::OnScriptedPrintReply,
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

// static
Clipboard* ResourceMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static Clipboard* clipboard = new Clipboard;

  return clipboard;
}

ChromeURLRequestContext* ResourceMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
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
    size_t size, bool cache_in_browser, TransportDIB::Handle* handle) {
  render_widget_helper_->AllocTransportDIB(size, cache_in_browser, handle);
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
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OpenChannelToExtensionOnUIThread,
          id(), routing_id, port2_id, source_extension_id,
          target_extension_id, channel_name));
}

void ResourceMessageFilter::OpenChannelToExtensionOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    const std::string& source_extension_id,
    const std::string& target_extension_id,
    const std::string& channel_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  profile_->GetExtensionMessageService()->OpenChannelToExtension(
      source_process_id, source_routing_id, receiver_port_id,
      source_extension_id, target_extension_id, channel_name);
}

void ResourceMessageFilter::OnOpenChannelToTab(
    int routing_id, int tab_id, const std::string& extension_id,
    const std::string& channel_name, int* port_id) {
  int port2_id;
  ExtensionMessageService::AllocatePortIdPair(port_id, &port2_id);

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OpenChannelToTabOnUIThread,
          id(), routing_id, port2_id, tab_id, extension_id, channel_name));
}

void ResourceMessageFilter::OpenChannelToTabOnUIThread(
    int source_process_id, int source_routing_id,
    int receiver_port_id,
    int tab_id,
    const std::string& extension_id,
    const std::string& channel_name) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  profile_->GetExtensionMessageService()->OpenChannelToTab(
      source_process_id, source_routing_id, receiver_port_id,
      tab_id, extension_id, channel_name);
}

bool ResourceMessageFilter::CheckBenchmarkingEnabled() const {
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
  net::HttpCache* http_cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  http_cache->set_mode(mode);
}

void ResourceMessageFilter::OnClearCache(IPC::Message* reply_msg) {
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

bool ResourceMessageFilter::CheckPreparsedJsCachingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnablePreparsedJsCaching);
    checked = true;
  }
  return result;
}

void ResourceMessageFilter::OnCacheableMetadataAvailable(
    const GURL& url,
    double expected_response_time,
    const std::vector<char>& data) {
  if (!CheckPreparsedJsCachingEnabled())
    return;

  net::HttpCache* cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  DCHECK(cache);

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(data.size());
  memcpy(buf->data(), &data.front(), data.size());
  cache->WriteMetadata(
      url, base::Time::FromDoubleT(expected_response_time), buf, data.size());
}

// TODO(lzheng): This only enables spdy over ssl. Enable spdy for http
// when needed.
void ResourceMessageFilter::OnEnableSpdy(bool enable) {
  if (enable) {
    net::HttpNetworkLayer::EnableSpdy("npn,force-alt-protocols");
  } else {
    net::HttpNetworkLayer::EnableSpdy("npn-http");
  }
}

void ResourceMessageFilter::OnGetFileSize(const FilePath& path,
                                          IPC::Message* reply_msg) {
  // Get file size only when the child process has been granted permission to
  // upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(id(), path)) {
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
          this, &ResourceMessageFilter::OnGetFileInfoOnFileThread, path,
          reply_msg, &WriteFileSize));
}

void ResourceMessageFilter::OnGetFileModificationTime(const FilePath& path,
                                                      IPC::Message* reply_msg) {
  // Get file modification time only when the child process has been granted
  // permission to upload the file.
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(id(), path)) {
    ViewHostMsg_GetFileModificationTime::WriteReplyParams(reply_msg,
                                                          base::Time());
    Send(reply_msg);
    return;
  }

  // Getting file modification time could take a long time if it lives on a
  // network share, so run it on the FILE thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetFileInfoOnFileThread,
          path, reply_msg, &WriteFileModificationTime));
}

void ResourceMessageFilter::OnGetFileInfoOnFileThread(
    const FilePath& path,
    IPC::Message* reply_msg,
    FileInfoWriteFunc write_func) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  base::PlatformFileInfo file_info;
  file_info.size = 0;
  file_util::GetFileInfo(path, &file_info);

  (*write_func)(reply_msg, file_info);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnOpenFile(const FilePath& path,
                                       int mode,
                                       IPC::Message* reply_msg) {
  // Open the file only when the child process has been granted permission to
  // upload the file.
  // TODO(jianli): Do we need separate permission to control opening the file?
  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(id(), path)) {
    ViewHostMsg_OpenFile::WriteReplyParams(
        reply_msg,
#if defined(OS_WIN)
        base::kInvalidPlatformFileValue
#elif defined(OS_POSIX)
        base::FileDescriptor(base::kInvalidPlatformFileValue, true)
#endif
        );
    Send(reply_msg);
    return;
  }

  // Opening the file could take a long time if it lives on a network share,
  // so run it on the FILE thread.
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnOpenFileOnFileThread,
          path, mode, reply_msg));
}

void ResourceMessageFilter::OnOpenFileOnFileThread(const FilePath& path,
                                                   int mode,
                                                   IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  base::PlatformFile file_handle = base::CreatePlatformFile(
      path,
      (mode == 0) ? (base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ)
                  : (base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE),
      NULL, NULL);

  base::PlatformFile target_file_handle;
#if defined(OS_WIN)
  // Duplicate the file handle so that the renderer process can access the file.
  if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                       handle(), &target_file_handle, 0, false,
                       DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    // file_handle is closed whether or not DuplicateHandle succeeds.
    target_file_handle = INVALID_HANDLE_VALUE;
  }
#else
  target_file_handle = file_handle;
#endif

  ViewHostMsg_OpenFile::WriteReplyParams(
      reply_msg,
#if defined(OS_WIN)
      target_file_handle
#elif defined(OS_POSIX)
      base::FileDescriptor(target_file_handle, true)
#endif
      );

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnKeygen(uint32 key_size_index,
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

  LOG(INFO) << "Dispatching keygen task to worker pool.";
  // Dispatch to worker pool, so we do not block the IO thread.
  if (!WorkerPool::PostTask(
           FROM_HERE,
           NewRunnableMethod(
               this, &ResourceMessageFilter::OnKeygenOnWorkerThread,
               key_size_in_bits, challenge_string, url, reply_msg),
           true)) {
    NOTREACHED() << "Failed to dispatch keygen task to worker pool";
    ViewHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
    return;
  }
}

void ResourceMessageFilter::OnKeygenOnWorkerThread(
    int key_size_in_bits,
    const std::string& challenge_string,
    const GURL& url,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);
  // Verify we are on a worker thread.
  DCHECK(!MessageLoop::current());

  // Generate a signed public key and challenge, then send it back.
  net::KeygenHandler keygen_handler(key_size_in_bits, challenge_string, url);

  ViewHostMsg_Keygen::WriteReplyParams(
      reply_msg,
      keygen_handler.GenKeyAndSignChallenge());

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

#if defined(USE_TCMALLOC)
void ResourceMessageFilter::OnRendererTcmalloc(base::ProcessId pid,
                                               const std::string& output) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableFunction(AboutTcmallocRendererCallback, pid, output));
}
#endif

void ResourceMessageFilter::OnEstablishGpuChannel() {
  GpuProcessHost::Get()->EstablishGpuChannel(id(), this);
}

void ResourceMessageFilter::OnSynchronizeGpu(IPC::Message* reply) {
  // We handle this message (and the other GPU process messages) here
  // rather than handing the message to the GpuProcessHost for
  // dispatch so that we can use the DELAY_REPLY macro to synthesize
  // the reply message, and also send down a "this" pointer so that
  // the GPU process host can send the reply later.
  GpuProcessHost::Get()->Synchronize(reply, this);
}

void ResourceMessageFilter::OnGetExtensionMessageBundle(
    const std::string& extension_id, IPC::Message* reply_msg) {
  ChromeURLRequestContext* context = static_cast<ChromeURLRequestContext*>(
    request_context_->GetURLRequestContext());

  FilePath extension_path =
      context->extension_info_map()->GetPathForExtension(extension_id);
  std::string default_locale =
      context->extension_info_map()->GetDefaultLocaleForExtension(extension_id);

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread,
          extension_path, extension_id, default_locale, reply_msg));
}

void ResourceMessageFilter::OnGetExtensionMessageBundleOnFileThread(
    const FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale,
    IPC::Message* reply_msg) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

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

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(this, &ResourceMessageFilter::Send, reply_msg));
}

void ResourceMessageFilter::OnAsyncOpenFile(const IPC::Message& msg,
                                            const FilePath& path,
                                            int flags,
                                            int message_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (!ChildProcessSecurityPolicy::GetInstance()->CanReadFile(id(), path)) {
    IPC::Message* reply = new ViewMsg_AsyncOpenFile_ACK(
        msg.routing_id(), base::PLATFORM_FILE_ERROR_ACCESS_DENIED,
        IPC::InvalidPlatformFileForTransit(), message_id);
    Send(reply);
    return;
  }

  // TODO(dumi): update this check once we have a security attribute
  // that allows renderers to modify files.
  int allowed_flags =
      base::PLATFORM_FILE_OPEN |
      base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_EXCLUSIVE_READ |
      base::PLATFORM_FILE_ASYNC;
  if (flags & ~allowed_flags) {
    DLOG(ERROR) << "Bad flags in ViewMsgHost_AsyncOpenFile message: " << flags;
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_AsyncOpenFile::ID, handle());
    return;
  }

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE, NewRunnableMethod(
          this, &ResourceMessageFilter::AsyncOpenFileOnFileThread,
          path, flags, message_id, msg.routing_id()));
}

void ResourceMessageFilter::AsyncOpenFileOnFileThread(const FilePath& path,
                                                      int flags,
                                                      int message_id,
                                                      int routing_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
  base::PlatformFile file = base::CreatePlatformFile(
      path, flags, NULL, &error_code);
  IPC::PlatformFileForTransit file_for_transit =
      IPC::InvalidPlatformFileForTransit();
  if (file != base::kInvalidPlatformFileValue) {
#if defined(OS_WIN)
    ::DuplicateHandle(::GetCurrentProcess(), file, handle(),
                      &file_for_transit, 0, false, DUPLICATE_SAME_ACCESS);
#else
    file_for_transit = base::FileDescriptor(file, true);
#endif
  }

  IPC::Message* reply = new ViewMsg_AsyncOpenFile_ACK(
      routing_id, error_code, file_for_transit, message_id);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE, NewRunnableMethod(
          this, &ResourceMessageFilter::Send, reply));
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

void SetCookieCompletion::RunWithParams(const Tuple1<int>& params) {
  int result = params.a;
  bool blocked_by_policy = true;
  if (result == net::OK ||
      result == net::OK_FOR_SESSION_ONLY) {
    blocked_by_policy = false;
    net::CookieOptions options;
    if (result == net::OK_FOR_SESSION_ONLY)
      options.set_force_session();
    context_->cookie_store()->SetCookieWithOptions(url_, cookie_line_,
                                                   options);
  }
  if (!context_->IsExternal()) {
    CallRenderViewHostContentSettingsDelegate(
        render_process_id_, render_view_id_,
        &RenderViewHostDelegate::ContentSettings::OnCookieAccessed,
        url_, cookie_line_, blocked_by_policy);
  }
  delete this;
}

GetCookiesCompletion::GetCookiesCompletion(int render_process_id,
                                           int render_view_id,
                                           const GURL& url,
                                           IPC::Message* reply_msg,
                                           ResourceMessageFilter* filter,
                                           URLRequestContext* context,
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

void GetCookiesCompletion::RunWithParams(const Tuple1<int>& params) {
  if (!raw_cookies_) {
    int result = params.a;
    std::string cookies;
    if (result == net::OK)
      cookies = cookie_store()->GetCookies(url_);
    ViewHostMsg_GetCookies::WriteReplyParams(reply_msg_, cookies);
    filter_->Send(reply_msg_);
    delete this;
  } else {
    // Ignore the policy result.  We only waited on the policy result so that
    // any pending 'set-cookie' requests could be flushed.  The intent of
    // querying the raw cookies is to reveal the contents of the cookie DB, so
    // it important that we don't read the cookie db ahead of pending writes.
    net::CookieMonster* cookie_monster =
        context_->cookie_store()->GetCookieMonster();
    net::CookieMonster::CookieList cookie_list =
        cookie_monster->GetAllCookiesForURL(url_);

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
