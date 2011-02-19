// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#pragma once

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/file_path.h"
#include "base/linked_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_message_filter.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/net/resolve_proxy_msg_helper.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/content_settings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/native_widget_types.h"

class ChromeURLRequestContext;
struct FontDescriptor;
class HostContentSettingsMap;
class HostZoomMap;
class NotificationsPrefsCache;
class Profile;
class RenderWidgetHelper;
class URLRequestContextGetter;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_CreateWorker_Params;

namespace webkit {
namespace npapi {
struct WebPluginInfo;
}
}

namespace base {
class SharedMemory;
}

namespace net {
class CookieStore;
}

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

struct ViewHostMsg_ScriptedPrint_Params;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class RenderMessageFilter : public BrowserMessageFilter,
                            public ResolveProxyMsgHelper::Delegate {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      PluginService* plugin_service,
                      Profile* profile,
                      RenderWidgetHelper* render_widget_helper);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
  virtual void OnDestruct() const;

  int render_process_id() const { return render_process_id_; }
  ResourceDispatcherHost* resource_dispatcher_host() {
    return resource_dispatcher_host_;
  }
  bool off_the_record() { return off_the_record_; }

  // Returns either the extension net::URLRequestContext or regular
  // net::URLRequestContext depending on whether |url| is an extension URL.
  // Only call on the IO thread.
  ChromeURLRequestContext* GetRequestContextForURL(const GURL& url);

 private:
  friend class BrowserThread;
  friend class DeleteTask<RenderMessageFilter>;

  virtual ~RenderMessageFilter();

  void OnMsgCreateWindow(const ViewHostMsg_CreateWindow_Params& params,
                         int* route_id,
                         int64* cloned_session_storage_namespace_id);
  void OnMsgCreateWidget(int opener_id,
                         WebKit::WebPopupType popup_type,
                         int* route_id);
  void OnMsgCreateFullscreenWidget(int opener_id, int* route_id);
  void OnSetCookie(const IPC::Message& message,
                   const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);
  void OnGetCookies(const GURL& url,
                    const GURL& first_party_for_cookies,
                    IPC::Message* reply_msg);
  void OnGetRawCookies(const GURL& url,
                       const GURL& first_party_for_cookies,
                       IPC::Message* reply_msg);
  void OnDeleteCookie(const GURL& url,
                      const std::string& cookieName);
  void OnCookiesEnabled(const GURL& url,
                        const GURL& first_party_for_cookies,
                        IPC::Message* reply_msg);
  void OnPluginFileDialog(const IPC::Message& msg,
                          bool multiple_files,
                          const std::wstring& title,
                          const std::wstring& filter,
                          uint32 user_data);

#if defined(OS_MACOSX)
  void OnLoadFont(const FontDescriptor& font,
                  uint32* handle_size,
                  base::SharedMemoryHandle* handle);
#endif

#if defined(OS_WIN)  // This hack is Windows-specific.
  // Cache fonts for the renderer. See RenderMessageFilter::OnPreCacheFont
  // implementation for more details.
  void OnPreCacheFont(LOGFONT font);
#endif

#if !defined(OS_MACOSX)
  // Not handled in the IO thread on Mac.
  void OnGetScreenInfo(gfx::NativeViewId window, IPC::Message* reply);
#endif
  void OnGetPlugins(bool refresh, IPC::Message* reply_msg);
  void OnGetPluginsOnFileThread(bool refresh, IPC::Message* reply_msg);
  void OnGetPluginInfo(int routing_id,
                       const GURL& url,
                       const GURL& policy_url,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);
  void OnGetPluginInfoOnFileThread(int render_view_id,
                                   const GURL& url,
                                   const GURL& policy_url,
                                   const std::string& mime_type,
                                   IPC::Message* reply_msg);
  void OnGotPluginInfo(bool found,
                       const webkit::npapi::WebPluginInfo& info,
                       const std::string& actual_mime_type,
                       const GURL& policy_url,
                       IPC::Message* reply_msg);
  void OnOpenChannelToPlugin(int routing_id,
                             const GURL& url,
                             const std::string& mime_type,
                             IPC::Message* reply_msg);
  void OnOpenChannelToPepperPlugin(const FilePath& path,
                                   IPC::Message* reply_msg);
  void OnLaunchNaCl(const std::wstring& url,
                    int channel_descriptor,
                    IPC::Message* reply_msg);
  void OnGenerateRoutingID(int* route_id);
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
  void OnClipboardWriteObjectsAsync(const ui::Clipboard::ObjectMap& objects);
  void OnClipboardWriteObjectsSync(const ui::Clipboard::ObjectMap& objects,
                                   base::SharedMemoryHandle bitmap_handle);

  void OnClipboardIsFormatAvailable(ui::Clipboard::FormatType format,
                                    ui::Clipboard::Buffer buffer,
                                    IPC::Message* reply);
  void OnClipboardReadText(ui::Clipboard::Buffer buffer, IPC::Message* reply);
  void OnClipboardReadAsciiText(ui::Clipboard::Buffer buffer,
                                IPC::Message* reply);
  void OnClipboardReadHTML(ui::Clipboard::Buffer buffer, IPC::Message* reply);
#if defined(OS_MACOSX)
  void OnClipboardFindPboardWriteString(const string16& text);
#endif
  void OnClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                     IPC::Message* reply);
  void OnClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                           IPC::Message* reply);
  void OnClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                                IPC::Message* reply);

  void OnCheckNotificationPermission(const GURL& source_url,
                                     int* permission_level);

#if !defined(OS_MACOSX)
  // Not handled in the IO thread on Mac.
  void OnGetWindowRect(gfx::NativeViewId window, IPC::Message* reply);
  void OnGetRootWindowRect(gfx::NativeViewId window, IPC::Message* reply);
#endif

  void OnRevealFolderInOS(const FilePath& path);
  void OnGetCPBrowsingContext(uint32* context);

#if defined(OS_WIN)
  // Used to pass resulting EMF from renderer to browser in printing.
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

#if defined(OS_CHROMEOS)
  // Used to ask the browser allocate a temporary file for the renderer
  // to fill in resulting PDF in renderer.
  void OnAllocateTempFileForPrinting(IPC::Message* reply_msg);
  void OnTempFileForPrintingWritten(int sequence_number);
#endif

#if defined(OS_POSIX)
  // Used to ask the browser to allocate a block of shared memory for the
  // renderer to send back data in, since shared memory can't be created
  // in the renderer on POSIX due to the sandbox.
  void OnAllocateSharedMemoryBuffer(uint32 buffer_size,
                                    base::SharedMemoryHandle* handle);
#endif

  void OnResourceTypeStats(const WebKit::WebCache::ResourceTypeStats& stats);
  static void OnResourceTypeStatsOnUIThread(
      const WebKit::WebCache::ResourceTypeStats&,
      base::ProcessId renderer_id);

  void OnV8HeapStats(int v8_memory_allocated, int v8_memory_used);
  static void OnV8HeapStatsOnUIThread(int v8_memory_allocated,
                                      int v8_memory_used,
                                      base::ProcessId renderer_id);

  void OnDidZoomURL(const IPC::Message& message,
                    double zoom_level,
                    bool remember,
                    const GURL& url);
  void UpdateHostZoomLevelsOnUIThread(double zoom_level,
                                      bool remember,
                                      const GURL& url,
                                      int render_process_id,
                                      int render_view_id);

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

  // A javascript code requested to print the current page. The renderer host
  // have to show to the user the print dialog and returns the selected print
  // settings.
  void OnScriptedPrint(const ViewHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      int routing_id,
      IPC::Message* reply_msg);

  // Browser side transport DIB allocation
  void OnAllocTransportDIB(size_t size,
                           bool cache_in_browser,
                           TransportDIB::Handle* result);
  void OnFreeTransportDIB(TransportDIB::Id dib_id);

  void OnOpenChannelToExtension(int routing_id,
                                const std::string& source_extension_id,
                                const std::string& target_extension_id,
                                const std::string& channel_name, int* port_id);
  void OpenChannelToExtensionOnUIThread(int source_process_id,
                                        int source_routing_id,
                                        int receiver_port_id,
                                        const std::string& source_extension_id,
                                        const std::string& target_extension_id,
                                        const std::string& channel_name);
  void OnOpenChannelToTab(int routing_id, int tab_id,
                          const std::string& extension_id,
                          const std::string& channel_name, int* port_id);
  void OpenChannelToTabOnUIThread(int source_process_id, int source_routing_id,
                                  int receiver_port_id,
                                  int tab_id, const std::string& extension_id,
                                  const std::string& channel_name);

  void OnCloseCurrentConnections();
  void OnSetCacheMode(bool enabled);
  void OnClearCache(bool preserve_ssl_host_info, IPC::Message* reply_msg);
  void OnCacheableMetadataAvailable(const GURL& url,
                                    double expected_response_time,
                                    const std::vector<char>& data);
  void OnEnableSpdy(bool enable);
  void OnKeygen(uint32 key_size_index, const std::string& challenge_string,
                const GURL& url, IPC::Message* reply_msg);
  void OnKeygenOnWorkerThread(
      int key_size_in_bits,
      const std::string& challenge_string,
      const GURL& url,
      IPC::Message* reply_msg);
  void OnGetExtensionMessageBundle(const std::string& extension_id,
                                   IPC::Message* reply_msg);
  void OnGetExtensionMessageBundleOnFileThread(
      const FilePath& extension_path,
      const std::string& extension_id,
      const std::string& default_locale,
      IPC::Message* reply_msg);
  void OnAsyncOpenFile(const IPC::Message& msg,
                       const FilePath& path,
                       int flags,
                       int message_id);
  void AsyncOpenFileOnFileThread(const FilePath& path,
                                 int flags,
                                 int message_id,
                                 int routing_id);

#if defined(USE_X11)
  void DoOnGetScreenInfo(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnGetRootWindowRect(gfx::NativeViewId view, IPC::Message* reply_msg);
  void DoOnClipboardIsFormatAvailable(ui::Clipboard::FormatType format,
                                      ui::Clipboard::Buffer buffer,
                                      IPC::Message* reply_msg);
  void DoOnClipboardReadText(ui::Clipboard::Buffer buffer,
                             IPC::Message* reply_msg);
  void DoOnClipboardReadAsciiText(ui::Clipboard::Buffer buffer,
                                  IPC::Message* reply_msg);
  void DoOnClipboardReadHTML(ui::Clipboard::Buffer buffer,
                             IPC::Message* reply_msg);
  void DoOnClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                       IPC::Message* reply_msg);
  void DoOnClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                             IPC::Message* reply_msg);
  void DoOnClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                                  IPC::Message* reply_msg);
  void DoOnAllocateTempFileForPrinting(IPC::Message* reply_msg);
  void DoOnTempFileForPrintingWritten(int sequence_number);
#endif

  bool CheckBenchmarkingEnabled() const;
  bool CheckPreparsedJsCachingEnabled() const;

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static ui::Clipboard* GetClipboard();

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHost* resource_dispatcher_host_;
  PluginService* plugin_service_;
  printing::PrintJobManager* print_job_manager_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;

  // The host content settings map. Stored separately from the profile so we can
  // access it on other threads.
  HostContentSettingsMap* content_settings_;

  // Helper class for handling PluginProcessHost_ResolveProxy messages (manages
  // the requests to the proxy service).
  ResolveProxyMsgHelper resolve_proxy_msg_helper_;

  // Contextual information to be used for requests created here.
  scoped_refptr<URLRequestContextGetter> request_context_;

  // A request context that holds a cookie store for chrome-extension URLs.
  scoped_refptr<URLRequestContextGetter> extensions_request_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // A cache of notifications preferences which is used to handle
  // Desktop Notifications permission messages.
  scoped_refptr<NotificationsPrefsCache> notification_prefs_;

  // Handles zoom-related messages.
  scoped_refptr<HostZoomMap> host_zoom_map_;

  // Whether this process is used for off the record tabs.
  bool off_the_record_;

  bool cloud_print_enabled_;

  base::TimeTicks last_plugin_refresh_time_;  // Initialized to 0.

  scoped_refptr<WebKitContext> webkit_context_;

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

// These classes implement completion callbacks for getting and setting
// cookies.
class SetCookieCompletion : public net::CompletionCallback {
 public:
  SetCookieCompletion(int render_process_id,
                      int render_view_id,
                      const GURL& url,
                      const std::string& cookie_line,
                      ChromeURLRequestContext* context);
  virtual ~SetCookieCompletion();

  virtual void RunWithParams(const Tuple1<int>& params);

  int render_process_id() const {
    return render_process_id_;
  }

  int render_view_id() const {
    return render_view_id_;
  }

 private:
  int render_process_id_;
  int render_view_id_;
  GURL url_;
  std::string cookie_line_;
  scoped_refptr<ChromeURLRequestContext> context_;
};

class GetCookiesCompletion : public net::CompletionCallback {
 public:
  GetCookiesCompletion(int render_process_id,
                       int render_view_id,
                       const GURL& url, IPC::Message* reply_msg,
                       RenderMessageFilter* filter,
                       ChromeURLRequestContext* context,
                       bool raw_cookies);
  virtual ~GetCookiesCompletion();

  virtual void RunWithParams(const Tuple1<int>& params);

  int render_process_id() const {
    return render_process_id_;
  }

  int render_view_id() const {
    return render_view_id_;
  }

  void set_cookie_store(net::CookieStore* cookie_store);

  net::CookieStore* cookie_store() {
    return cookie_store_.get();
  }

 private:
  GURL url_;
  IPC::Message* reply_msg_;
  scoped_refptr<RenderMessageFilter> filter_;
  scoped_refptr<ChromeURLRequestContext> context_;
  int render_process_id_;
  int render_view_id_;
  bool raw_cookies_;
  scoped_refptr<net::CookieStore> cookie_store_;
};

class CookiesEnabledCompletion : public net::CompletionCallback {
 public:
  CookiesEnabledCompletion(IPC::Message* reply_msg,
                           RenderMessageFilter* filter);
  virtual ~CookiesEnabledCompletion();

  virtual void RunWithParams(const Tuple1<int>& params);

 private:
  IPC::Message* reply_msg_;
  scoped_refptr<RenderMessageFilter> filter_;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
