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

#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "content/browser/browser_message_filter.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/surface/transport_dib.h"

struct FontDescriptor;
class PluginService;
class RenderWidgetHelper;
struct ViewHostMsg_CreateWindow_Params;

namespace WebKit {
struct WebScreenInfo;
}

namespace content {
class BrowserContext;
class ResourceContext;
}

namespace base {
class ProcessMetrics;
class SharedMemory;
}

namespace gfx {
class Rect;
}

namespace media {
struct MediaLogEvent;
}

namespace net {
class CookieList;
class URLRequestContextGetter;
}

namespace webkit {
struct WebPluginInfo;
}

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class RenderMessageFilter : public BrowserMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      PluginService* plugin_service,
                      content::BrowserContext* browser_context,
                      net::URLRequestContextGetter* request_context,
                      RenderWidgetHelper* render_widget_helper);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

  bool OffTheRecord() const;

  int render_process_id() const { return render_process_id_; }
  ResourceDispatcherHost* resource_dispatcher_host() {
    return resource_dispatcher_host_;
  }

  // Returns the correct net::URLRequestContext depending on what type of url is
  // given.
  // Only call on the IO thread.
  net::URLRequestContext* GetRequestContextForURL(const GURL& url);

 private:
  friend class content::BrowserThread;
  friend class DeleteTask<RenderMessageFilter>;

  class OpenChannelToNpapiPluginCallback;

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
                        bool* cookies_enabled);

#if defined(OS_MACOSX)
  void OnLoadFont(const FontDescriptor& font,
                  uint32* handle_size,
                  base::SharedMemoryHandle* handle,
                  uint32* font_id);
#endif

#if defined(OS_WIN) && !defined(USE_AURA)
  // On Windows, we handle these on the IO thread to avoid a deadlock with
  // plugins.  On non-Windows systems, we need to handle them on the UI thread.
  void OnGetScreenInfo(gfx::NativeViewId window,
                       WebKit::WebScreenInfo* results);
  void OnGetWindowRect(gfx::NativeViewId window, gfx::Rect* rect);
  void OnGetRootWindowRect(gfx::NativeViewId window, gfx::Rect* rect);
#endif

  void OnGetPlugins(bool refresh, IPC::Message* reply_msg);
  void GetPluginsCallback(IPC::Message* reply_msg,
                          const std::vector<webkit::WebPluginInfo>& plugins);
  void OnGetPluginInfo(int routing_id,
                       const GURL& url,
                       const GURL& policy_url,
                       const std::string& mime_type,
                       bool* found,
                       webkit::WebPluginInfo* info,
                       std::string* actual_mime_type);
  void OnOpenChannelToPlugin(int routing_id,
                             const GURL& url,
                             const GURL& policy_url,
                             const std::string& mime_type,
                             IPC::Message* reply_msg);
  void OnOpenChannelToPepperPlugin(const FilePath& path,
                                   IPC::Message* reply_msg);
  void OnOpenChannelToPpapiBroker(int routing_id,
                                  int request_id,
                                  const FilePath& path);
  void OnGenerateRoutingID(int* route_id);
  void OnDownloadUrl(const IPC::Message& message,
                     const GURL& url,
                     const GURL& referrer,
                     const string16& suggested_name);
  void OnCheckNotificationPermission(const GURL& source_origin,
                                     int* permission_level);

  void OnGetCPUUsage(int* cpu_usage);

  void OnGetHardwareBufferSize(uint32* buffer_size);
  void OnGetHardwareInputSampleRate(double* sample_rate);
  void OnGetHardwareSampleRate(double* sample_rate);

  // Used to ask the browser to allocate a block of shared memory for the
  // renderer to send back data in, since shared memory can't be created
  // in the renderer on POSIX due to the sandbox.
  void OnAllocateSharedMemory(uint32 buffer_size,
                              base::SharedMemoryHandle* handle);
  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

  // Browser side transport DIB allocation
  void OnAllocTransportDIB(size_t size,
                           bool cache_in_browser,
                           TransportDIB::Handle* result);
  void OnFreeTransportDIB(TransportDIB::Id dib_id);
  void OnCacheableMetadataAvailable(const GURL& url,
                                    double expected_response_time,
                                    const std::vector<char>& data);
  void OnKeygen(uint32 key_size_index, const std::string& challenge_string,
                const GURL& url, IPC::Message* reply_msg);
  void OnKeygenOnWorkerThread(
      int key_size_in_bits,
      const std::string& challenge_string,
      const GURL& url,
      IPC::Message* reply_msg);
  void OnAsyncOpenFile(const IPC::Message& msg,
                       const FilePath& path,
                       int flags,
                       int message_id);
  void AsyncOpenFileOnFileThread(const FilePath& path,
                                 int flags,
                                 int message_id,
                                 int routing_id);
  void OnMediaLogEvent(const media::MediaLogEvent&);

  // Check the policy for getting cookies. Gets the cookies if allowed.
  void CheckPolicyForCookies(const GURL& url,
                             const GURL& first_party_for_cookies,
                             IPC::Message* reply_msg,
                             const net::CookieList& cookie_list);

  // Writes the cookies to reply messages, and sends the message.
  // Callback functions for getting cookies from cookie store.
  void SendGetCookiesResponse(IPC::Message* reply_msg,
                              const std::string& cookies);
  void SendGetRawCookiesResponse(IPC::Message* reply_msg,
                                 const net::CookieList& cookie_list);

  bool CheckBenchmarkingEnabled() const;
  bool CheckPreparsedJsCachingEnabled() const;
  void OnCompletedOpenChannelToNpapiPlugin(
      OpenChannelToNpapiPluginCallback* client);

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHost* resource_dispatcher_host_;
  PluginService* plugin_service_;

  // The browser context associated with our renderer process.  This should only
  // be accessed on the UI thread!
  content::BrowserContext* browser_context_;

  // Contextual information to be used for requests created here.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The ResourceContext which is to be used on the IO thread.
  const content::ResourceContext& resource_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Whether this process is used for incognito tabs.
  bool incognito_;

  // Initialized to 0, accessed on FILE thread only.
  base::TimeTicks last_plugin_refresh_time_;

  scoped_refptr<WebKitContext> webkit_context_;

  int render_process_id_;

  std::set<OpenChannelToNpapiPluginCallback*> plugin_host_clients_;

  // Records the last time we sampled CPU usage of the renderer process.
  base::TimeTicks cpu_usage_sample_time_;
  // Records the last sampled CPU usage in percents.
  int cpu_usage_;
  // Used for sampling CPU usage of the renderer process.
  scoped_ptr<base::ProcessMetrics> process_metrics_;

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
