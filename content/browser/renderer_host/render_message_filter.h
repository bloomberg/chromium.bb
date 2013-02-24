// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/shared_memory.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/three_d_api_types.h"
#include "media/base/channel_layout.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_loader.h"
#endif

struct FontDescriptor;
struct ViewHostMsg_CreateWindow_Params;

namespace WebKit {
struct WebScreenInfo;
}

namespace base {
class ProcessMetrics;
class SharedMemory;
class TaskRunner;
}

namespace gfx {
class Rect;
}

namespace media {
struct MediaLogEvent;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}

namespace webkit {
struct WebPluginInfo;
}

namespace content {
class BrowserContext;
class DOMStorageContextImpl;
class MediaInternals;
class PluginServiceImpl;
class RenderWidgetHelper;
class ResourceContext;
class ResourceDispatcherHostImpl;
struct Referrer;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class RenderMessageFilter : public BrowserMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      PluginServiceImpl * plugin_service,
                      BrowserContext* browser_context,
                      net::URLRequestContextGetter* request_context,
                      RenderWidgetHelper* render_widget_helper,
                      MediaInternals* media_internals,
                      DOMStorageContextImpl* dom_storage_context);

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;

  bool OffTheRecord() const;

  int render_process_id() const { return render_process_id_; }

  // Returns the correct net::URLRequestContext depending on what type of url is
  // given.
  // Only call on the IO thread.
  net::URLRequestContext* GetRequestContextForURL(const GURL& url);

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderMessageFilter>;

  class OpenChannelToNpapiPluginCallback;

  virtual ~RenderMessageFilter();

  void OnGetProcessMemorySizes(size_t* private_bytes, size_t* shared_bytes);
  void OnCreateWindow(const ViewHostMsg_CreateWindow_Params& params,
                      int* route_id,
                      int* surface_id,
                      int64* cloned_session_storage_namespace_id);
  void OnCreateWidget(int opener_id,
                      WebKit::WebPopupType popup_type,
                      int* route_id,
                      int* surface_id);
  void OnCreateFullscreenWidget(int opener_id,
                                int* route_id,
                                int* surface_id);
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
  // Messages for OOP font loading.
  void OnLoadFont(const FontDescriptor& font, IPC::Message* reply_msg);
  void SendLoadFontReply(IPC::Message* reply, FontLoader::Result* result);
#endif

#if defined(OS_WIN)
  void OnPreCacheFontCharacters(const LOGFONT& log_font,
                                const string16& characters);
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
  void OnOpenChannelToPepperPlugin(const base::FilePath& path,
                                   IPC::Message* reply_msg);
  void OnDidCreateOutOfProcessPepperInstance(
      int plugin_child_id,
      int32 pp_instance,
      PepperRendererInstanceData instance_data,
      bool is_external);
  void OnDidDeleteOutOfProcessPepperInstance(int plugin_child_id,
                                             int32 pp_instance,
                                             bool is_external);
  void OnOpenChannelToPpapiBroker(int routing_id,
                                  int request_id,
                                  const base::FilePath& path);
  void OnGenerateRoutingID(int* route_id);
  void OnDownloadUrl(const IPC::Message& message,
                     const GURL& url,
                     const Referrer& referrer,
                     const string16& suggested_name);
  void OnCheckNotificationPermission(const GURL& source_origin,
                                     int* permission_level);

  void OnGetCPUUsage(int* cpu_usage);

  void OnGetAudioHardwareConfig(int* output_buffer_size,
                                int* output_sample_rate, int* input_sample_rate,
                                media::ChannelLayout* input_channel_layout);

  // Used to look up the monitor color profile.
  void OnGetMonitorColorProfile(std::vector<char>* profile);

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
                       const base::FilePath& path,
                       int flags,
                       int message_id);
  void AsyncOpenFileOnFileThread(const base::FilePath& path,
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

  void OnUpdateIsDelayed(const IPC::Message& msg);
  void OnAre3DAPIsBlocked(int render_view_id,
                          const GURL& top_origin_url,
                          ThreeDAPIType requester,
                          bool* blocked);
  void OnDidLose3DContext(const GURL& top_origin_url,
                          ThreeDAPIType context_type,
                          int arb_robustness_status_code);

  // Cached resource request dispatcher host and plugin service, guaranteed to
  // be non-null if Init succeeds. We do not own the objects, they are managed
  // by the BrowserProcess, which has a wider scope than we do.
  ResourceDispatcherHostImpl* resource_dispatcher_host_;
  PluginServiceImpl* plugin_service_;
  base::FilePath profile_data_directory_;

  // Contextual information to be used for requests created here.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Whether this process is used for incognito contents.
  // This doesn't belong here; http://crbug.com/89628
  bool incognito_;

  // Initialized to 0, accessed on FILE thread only.
  base::TimeTicks last_plugin_refresh_time_;

  scoped_refptr<DOMStorageContextImpl> dom_storage_context_;

  int render_process_id_;

  std::set<OpenChannelToNpapiPluginCallback*> plugin_host_clients_;

  // Records the last time we sampled CPU usage of the renderer process.
  base::TimeTicks cpu_usage_sample_time_;
  // Records the last sampled CPU usage in percents.
  int cpu_usage_;
  // Used for sampling CPU usage of the renderer process.
  scoped_ptr<base::ProcessMetrics> process_metrics_;

  MediaInternals* media_internals_;

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
