// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class CookieSettings;
struct ExtensionHostMsg_Request_Params;
class ExtensionInfoMap;
class GURL;

namespace nacl {
struct NaClLaunchParams;
}

namespace net {
class HostResolver;
class URLRequestContextGetter;
}

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class ChromeRenderMessageFilter : public content::BrowserMessageFilter {
 public:
  ChromeRenderMessageFilter(int render_process_id,
                            Profile* profile,
                            net::URLRequestContextGetter* request_context);

  // Notification detail classes.
  class FPSDetails {
   public:
    FPSDetails(int routing_id, float fps)
        : routing_id_(routing_id),
          fps_(fps) {}
    int routing_id() const { return routing_id_; }
    float fps() const { return fps_; }
   private:
    int routing_id_;
    float fps_;
  };

  class V8HeapStatsDetails {
   public:
    V8HeapStatsDetails(size_t v8_memory_allocated,
                       size_t v8_memory_used)
        : v8_memory_allocated_(v8_memory_allocated),
          v8_memory_used_(v8_memory_used) {}
    size_t v8_memory_allocated() const { return v8_memory_allocated_; }
    size_t v8_memory_used() const { return v8_memory_used_; }
   private:
    size_t v8_memory_allocated_;
    size_t v8_memory_used_;
  };

  // content::BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;

  int render_process_id() { return render_process_id_; }
  bool off_the_record() { return off_the_record_; }
  net::HostResolver* GetHostResolver();

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ChromeRenderMessageFilter>;

  virtual ~ChromeRenderMessageFilter();

#if !defined(DISABLE_NACL)
  void OnLaunchNaCl(const nacl::NaClLaunchParams& launch_params,
                    IPC::Message* reply_msg);
  void OnGetReadonlyPnaclFd(const std::string& filename,
                            IPC::Message* reply_msg);
  void OnNaClCreateTemporaryFile(IPC::Message* reply_msg);
  void OnNaClErrorStatus(int render_view_id, int error_id);
#endif
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnResourceTypeStats(const WebKit::WebCache::ResourceTypeStats& stats);
  void OnUpdatedCacheStats(const WebKit::WebCache::UsageStats& stats);
  void OnFPS(int routing_id, float fps);
  void OnV8HeapStats(int v8_memory_allocated, int v8_memory_used);
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
  void OnOpenChannelToNativeApp(int routing_id,
                                const std::string& source_extension_id,
                                const std::string& native_app_name,
                                int* port_id);
  void OpenChannelToNativeAppOnUIThread(int source_routing_id,
                                        int receiver_port_id,
                                        const std::string& source_extension_id,
                                        const std::string& native_app_name);
  void OnOpenChannelToTab(int routing_id, int tab_id,
                          const std::string& extension_id,
                          const std::string& channel_name, int* port_id);
  void OpenChannelToTabOnUIThread(int source_process_id, int source_routing_id,
                                  int receiver_port_id,
                                  int tab_id, const std::string& extension_id,
                                  const std::string& channel_name);
  void OnGetExtensionMessageBundle(const std::string& extension_id,
                                   IPC::Message* reply_msg);
  void OnGetExtensionMessageBundleOnFileThread(
      const base::FilePath& extension_path,
      const std::string& extension_id,
      const std::string& default_locale,
      IPC::Message* reply_msg);
  void OnExtensionAddListener(const std::string& extension_id,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const std::string& event_name);
  void OnExtensionAddLazyListener(const std::string& extension_id,
                                  const std::string& event_name);
  void OnExtensionRemoveLazyListener(const std::string& extension_id,
                                     const std::string& event_name);
  void OnExtensionAddFilteredListener(const std::string& extension_id,
                                      const std::string& event_name,
                                      const base::DictionaryValue& filter,
                                      bool lazy);
  void OnExtensionRemoveFilteredListener(const std::string& extension_id,
                                         const std::string& event_name,
                                         const base::DictionaryValue& filter,
                                         bool lazy);
  void OnExtensionCloseChannel(int port_id, bool connection_error);
  void OnExtensionRequestForIOThread(
      int routing_id,
      const ExtensionHostMsg_Request_Params& params);
  void OnExtensionShouldSuspendAck(const std::string& extension_id,
                                   int sequence_id);
  void OnExtensionSuspendAck(const std::string& extension_id);
  void OnExtensionGenerateUniqueID(int* unique_id);
  void OnExtensionResumeRequests(int route_id);
  void OnAllowDatabase(int render_view_id,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       const string16& name,
                       const string16& display_name,
                       bool* allowed);
  void OnAllowDOMStorage(int render_view_id,
                         const GURL& origin_url,
                         const GURL& top_origin_url,
                         bool local,
                         bool* allowed);
  void OnAllowFileSystem(int render_view_id,
                         const GURL& origin_url,
                         const GURL& top_origin_url,
                         bool* allowed);
  void OnAllowIndexedDB(int render_view_id,
                        const GURL& origin_url,
                        const GURL& top_origin_url,
                        const string16& name,
                        bool* allowed);
  void OnCanTriggerClipboardRead(const GURL& origin, bool* allowed);
  void OnCanTriggerClipboardWrite(const GURL& origin, bool* allowed);
  void OnGetCookies(const GURL& url,
                    const GURL& first_party_for_cookies,
                    IPC::Message* reply_msg);
  void OnSetCookie(const IPC::Message& message,
                   const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& cookie);

  int render_process_id_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;
  // Copied from the profile so that it can be read on the IO thread.
  bool off_the_record_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  // Used to look up permissions at database creation time.
  scoped_refptr<CookieSettings> cookie_settings_;

  base::WeakPtrFactory<ChromeRenderMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
