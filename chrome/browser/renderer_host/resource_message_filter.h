// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "app/clipboard/clipboard.h"
#include "app/gfx/native_widget_types.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/net/resolve_proxy_msg_helper.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/translation_service.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/transport_dib.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"

class AppCacheDispatcherHost;
class AudioRendererHost;
class ChromeURLRequestContext;
class DatabaseDispatcherHost;
class DOMStorageDispatcherHost;
class ExtensionMessageService;
class HostZoomMap;
class NotificationsPrefsCache;
class Profile;
class RenderWidgetHelper;
class URLRequestContextGetter;
struct ViewHostMsg_Audio_CreateStream;
struct ViewHostMsg_CreateWorker_Params;
struct WebPluginInfo;

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

namespace webkit_glue {
struct WebCookie;
}

namespace WebKit {
struct WebScreenInfo;
}

struct ViewHostMsg_ScriptedPrint_Params;
#if defined(OS_LINUX)
struct ViewHostMsg_DidPrintPage_Params;
#endif

// This class filters out incoming IPC messages for network requests and
// processes them on the IPC thread.  As a result, network requests are not
// delayed by costly UI processing that may be occuring on the main thread of
// the browser.  It also means that any hangs in starting a network request
// will not interfere with browser UI.

class ResourceMessageFilter : public IPC::ChannelProxy::MessageFilter,
                              public ResourceDispatcherHost::Receiver,
                              public NotificationObserver,
                              public ResolveProxyMsgHelper::Delegate {
 public:
  // Create the filter.
  ResourceMessageFilter(ResourceDispatcherHost* resource_dispatcher_host,
                        int child_id,
                        AudioRendererHost* audio_renderer_host,
                        PluginService* plugin_service,
                        printing::PrintJobManager* print_job_manager,
                        Profile* profile,
                        RenderWidgetHelper* render_widget_helper,
                        URLRequestContextGetter* request_context);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();
  virtual void OnChannelClosing();
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnDestruct();

  // ResourceDispatcherHost::Receiver methods:
  virtual bool Send(IPC::Message* message);
  virtual URLRequestContext* GetRequestContext(
      uint32 request_id,
      const ViewHostMsg_Resource_Request& request_data);

  ResourceDispatcherHost* resource_dispatcher_host() {
    return resource_dispatcher_host_;
  }
  bool off_the_record() { return off_the_record_; }
  CallbackWithReturnValue<int>::Type* next_route_id_callback() {
    return next_route_id_callback_.get();
  }

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  friend class ChromeThread;
  friend class DeleteTask<ResourceMessageFilter>;

  virtual ~ResourceMessageFilter();

  void OnMsgCreateWindow(int opener_id, bool user_gesture,
                         int64 session_storage_namespace_id, int* route_id,
                         int64* cloned_session_storage_namespace_id);
  void OnMsgCreateWidget(int opener_id, bool activatable, int* route_id);
  void OnSetCookie(const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url,
                    const GURL& first_party_for_cookies,
                    std::string* cookies);
  void OnGetRawCookies(const GURL& url,
                       const GURL& first_party_for_cookies,
                       std::vector<webkit_glue::WebCookie>* raw_cookies);
  void OnDeleteCookie(const GURL& url,
                      const std::string& cookieName);
  void OnGetCookiesEnabled(const GURL& url,
                           const GURL& first_party_for_cookies,
                           bool* enabled);
  void OnPluginFileDialog(const IPC::Message& msg,
                          bool multiple_files,
                          const std::wstring& title,
                          const std::wstring& filter,
                          uint32 user_data);

#if defined(OS_WIN)  // This hack is Windows-specific.
  // Cache fonts for the renderer. See ResourceMessageFilter::OnLoadFont
  // implementation for more details
  void OnLoadFont(LOGFONT font);
#endif

#if !defined(OS_MACOSX)
  // Not handled in the IO thread on Mac.
  void OnGetScreenInfo(gfx::NativeViewId window, IPC::Message* reply);
#endif
  void OnGetPlugins(bool refresh, IPC::Message* reply_msg);
  void OnGetPluginsOnFileThread(bool refresh, IPC::Message* reply_msg);
  void OnGetPluginPath(const GURL& url,
                       const GURL& policy_url,
                       const std::string& mime_type,
                       FilePath* filename,
                       std::string* actual_mime_type);
  void OnOpenChannelToPlugin(const GURL& url,
                             const std::string& mime_type,
                             const std::wstring& locale,
                             IPC::Message* reply_msg);
  void OnLaunchNaCl(const std::wstring& url,
                    int channel_descriptor,
                    IPC::Message* reply_msg);
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnLookupSharedWorker(const GURL& url,
                            const string16& name,
                            unsigned long long document_id,
                            int render_view_route_id,
                            int* route_id,
                            bool* url_error);
  void OnDocumentDetached(unsigned long long document_id);
  void OnCancelCreateDedicatedWorker(int route_id);
  void OnForwardToWorker(const IPC::Message& msg);
  void OnDownloadUrl(const IPC::Message& message,
                     const GURL& url,
                     const GURL& referrer);
  void OnPlatformCheckSpelling(const string16& word, int tag, bool* correct);
  void OnPlatformFillSuggestionList(const string16& word,
                                    std::vector<string16>* suggestions);
  void OnGetDocumentTag(IPC::Message* reply_msg);
  void OnDocumentWithTagClosed(int tag);
  void OnShowSpellingPanel(bool show);
  void OnUpdateSpellingPanelWithMisspelledWord(const string16& word);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnRendererHistograms(int sequence_number,
                            const std::vector<std::string>& histogram_info);
#if defined(USE_TCMALLOC)
  void OnRendererTcmalloc(base::ProcessId pid, const std::string& output);
#endif
  void OnReceiveContextMenuMsg(const IPC::Message& msg);
  // Clipboard messages
  void OnClipboardWriteObjectsAsync(const Clipboard::ObjectMap& objects);
  void OnClipboardWriteObjectsSync(const Clipboard::ObjectMap& objects,
                                   base::SharedMemoryHandle bitmap_handle);

  void OnClipboardIsFormatAvailable(Clipboard::FormatType format,
                                    Clipboard::Buffer buffer,
                                    IPC::Message* reply);
  void OnClipboardReadText(Clipboard::Buffer buffer, IPC::Message* reply);
  void OnClipboardReadAsciiText(Clipboard::Buffer buffer, IPC::Message* reply);
  void OnClipboardReadHTML(Clipboard::Buffer buffer, IPC::Message* reply);
#if defined(OS_MACOSX)
  void OnClipboardFindPboardWriteString(const string16& text);
#endif

  void OnCheckNotificationPermission(const GURL& source_url,
                                     const std::string& application_id,
                                     int* permission_level);

#if !defined(OS_MACOSX)
  // Not handled in the IO thread on Mac.
  void OnGetWindowRect(gfx::NativeViewId window, IPC::Message* reply);
  void OnGetRootWindowRect(gfx::NativeViewId window, IPC::Message* reply);
#endif
  void OnGetMimeTypeFromExtension(const FilePath::StringType& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const FilePath& file_path,
                             std::string* mime_type);
  void OnGetPreferredExtensionForMimeType(const std::string& mime_type,
                                          FilePath::StringType* ext);
  void OnGetCPBrowsingContext(uint32* context);

#if defined(OS_WIN)
  // Used to pass resulting EMF from renderer to browser in printing.
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

#if defined(OS_LINUX)
  // Used to ask the browser allocate a temporary file for the renderer
  // to fill in resulting PDF in renderer.
  void OnAllocateTempFileForPrinting(IPC::Message* reply_msg);
  void OnTempFileForPrintingWritten(int fd_in_browser);
#endif
#if defined(OS_MACOSX)
  // Used to ask the browser to allocate a block of shared memory for the
  // renderer to send back data in, since shared memory can't be created
  // in the renderer on OS X due to the sandbox.
  void OnAllocateSharedMemoryBuffer(size_t buffer_size,
                              base::SharedMemoryHandle* handle);
#endif

  void OnResourceTypeStats(const WebKit::WebCache::ResourceTypeStats& stats);
  static void OnResourceTypeStatsOnUIThread(WebKit::WebCache::ResourceTypeStats,
                                            base::ProcessId renderer_id);

  void OnV8HeapStats(int v8_memory_allocated, int v8_memory_used);
  static void OnV8HeapStatsOnUIThread(int v8_memory_allocated,
                                      int v8_memory_used,
                                      base::ProcessId renderer_id);

  void OnDidZoomHost(const std::string& host, int zoom_level);
  void UpdateHostZoomLevelsOnUIThread(const std::string& host, int zoom_level);

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

  // ResolveProxyMsgHelper::Delegate implementation:
  virtual void OnResolveProxyCompleted(IPC::Message* reply_msg,
                                       int result,
                                       const std::string& proxy_list);

  // A javascript code requested to print the current page. This is done in two
  // steps and this is the first step. Get the print setting right here
  // synchronously. It will hang the I/O completely.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);
#if defined(OS_WIN) || defined(OS_MACOSX)
  // A javascript code requested to print the current page. The renderer host
  // have to show to the user the print dialog and returns the selected print
  // settings.
  void OnScriptedPrint(const ViewHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      int routing_id,
      IPC::Message* reply_msg);
#endif
  // Browser side transport DIB allocation
  void OnAllocTransportDIB(size_t size,
                           TransportDIB::Handle* result);
  void OnFreeTransportDIB(TransportDIB::Id dib_id);

  void OnOpenChannelToExtension(int routing_id,
                                const std::string& source_extension_id,
                                const std::string& target_extension_id,
                                const std::string& channel_name, int* port_id);
  void OnOpenChannelToTab(int routing_id, int tab_id,
                          const std::string& extension_id,
                          const std::string& channel_name, int* port_id);

  void OnCloseCurrentConnections();
  void OnSetCacheMode(bool enabled);

  void OnGetFileSize(const FilePath& path, IPC::Message* reply_msg);
  void OnGetFileSizeOnFileThread(const FilePath& path, IPC::Message* reply_msg);
  void OnKeygen(uint32 key_size_index, const std::string& challenge_string,
                const GURL& url, std::string* signed_public_key);
  void OnGetExtensionMessageBundle(const std::string& extension_id,
                                   IPC::Message* reply_msg);
  void OnGetExtensionMessageBundleOnFileThread(
      const FilePath& extension_path,
      const std::string& default_locale,
      IPC::Message* reply_msg);

  void OnTranslateText(ViewHostMsg_TranslateTextParam param);

#if defined(OS_LINUX)
  void SendDelayedReply(IPC::Message* reply_msg);
  void DoOnGetScreenInfo(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetRootWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnClipboardIsFormatAvailable(Clipboard::FormatType format,
                                      Clipboard::Buffer buffer,
                                      IPC::Message* reply_msg);
  void DoOnClipboardReadText(Clipboard::Buffer buffer, IPC::Message* reply_msg);
  void DoOnClipboardReadAsciiText(Clipboard::Buffer buffer,
                                  IPC::Message* reply_msg);
  void DoOnClipboardReadHTML(Clipboard::Buffer buffer, IPC::Message* reply_msg);
  void DoOnAllocateTempFileForPrinting(IPC::Message* reply_msg);
#endif

  bool CheckBenchmarkingEnabled();

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static Clipboard* GetClipboard();

  // Returns either the extension URLRequestContext or regular URLRequestContext
  // depending on whether |url| is an extension URL.
  ChromeURLRequestContext* GetRequestContextForURL(const GURL& url);

  NotificationRegistrar registrar_;

  // The channel associated with the renderer connection. This pointer is not
  // owned by this class.
  IPC::Channel* channel_;

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHost* resource_dispatcher_host_;
  PluginService* plugin_service_;
  printing::PrintJobManager* print_job_manager_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;

  // Helper class for handling PluginProcessHost_ResolveProxy messages (manages
  // the requests to the proxy service).
  ResolveProxyMsgHelper resolve_proxy_msg_helper_;

  // Contextual information to be used for requests created here.
  scoped_refptr<URLRequestContextGetter> request_context_;

  // A request context specific for media resources.
  scoped_refptr<URLRequestContextGetter> media_request_context_;

  // A request context that holds a cookie store for chrome-extension URLs.
  scoped_refptr<URLRequestContextGetter> extensions_request_context_;

  // Used for routing extension messages.
  scoped_refptr<ExtensionMessageService> extensions_message_service_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Object that should take care of audio related resource requests.
  scoped_refptr<AudioRendererHost> audio_renderer_host_;

  // Handles AppCache related messages.
  scoped_ptr<AppCacheDispatcherHost> appcache_dispatcher_host_;

  // Handles DOM Storage related messages.
  scoped_refptr<DOMStorageDispatcherHost> dom_storage_dispatcher_host_;

  // Handles HTML5 DB related messages
  scoped_refptr<DatabaseDispatcherHost> db_dispatcher_host_;

  // A cache of notifications preferences which is used to handle
  // Desktop Notifications permission messages.
  scoped_refptr<NotificationsPrefsCache> notification_prefs_;

  // Handles zoom-related messages.
  scoped_refptr<HostZoomMap> host_zoom_map_;

  // Whether this process is used for off the record tabs.
  bool off_the_record_;

  // A callback to create a routing id for the associated renderer process.
  scoped_ptr<CallbackWithReturnValue<int>::Type> next_route_id_callback_;

  // Used to translate page contents from one language to another.
  TranslationService translation_service_;

  DISALLOW_COPY_AND_ASSIGN(ResourceMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_RESOURCE_MESSAGE_FILTER_H_
