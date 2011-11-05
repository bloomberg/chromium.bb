// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "chrome/common/content_settings.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_message_filter.h"
#include "content/common/dom_storage_common.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

struct ChromeViewHostMsg_GetPluginInfo_Status;
class ContentSettingsPattern;
class CookieSettings;
struct ExtensionHostMsg_Request_Params;
class ExtensionInfoMap;
class FilePath;
class GURL;
class HostContentSettingsMap;

namespace net {
class URLRequestContextGetter;
}

namespace webkit {
struct WebPluginInfo;
}

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class ChromeRenderMessageFilter : public BrowserMessageFilter {
 public:
  ChromeRenderMessageFilter(int render_process_id,
                            Profile* profile,
                            net::URLRequestContextGetter* request_context);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
  virtual void OnDestruct() const;
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        content::BrowserThread::ID* thread);

 private:
  friend class content::BrowserThread;
  friend class DeleteTask<ChromeRenderMessageFilter>;

  struct GetPluginInfo_Params;

  virtual ~ChromeRenderMessageFilter();

  void OnLaunchNaCl(const std::wstring& url,
                    int channel_descriptor,
                    IPC::Message* reply_msg);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnRendererHistograms(int sequence_number,
                            const std::vector<std::string>& histogram_info);
  void OnRendererTrackedData(int sequence_number,
                             const std::string& tracked_data);
  void OnIsTrackingEnabled();
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
      const FilePath& extension_path,
      const std::string& extension_id,
      const std::string& default_locale,
      IPC::Message* reply_msg);
  void OnExtensionAddListener(const std::string& extension_id,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const std::string& event_name);
  void OnExtensionIdle(const std::string& extension_id);
  void OnExtensionEventAck(const std::string& extension_id);
  void OnExtensionCloseChannel(int port_id);
  void OnExtensionRequestForIOThread(
      int routing_id,
      const ExtensionHostMsg_Request_Params& params);
#if defined(USE_TCMALLOC)
  void OnRendererTcmalloc(const std::string& output);
  void OnWriteTcmallocHeapProfile(const FilePath::StringType& filename,
                                  const std::string& output);
#endif
  void OnGetPluginPolicies(ContentSetting* outdated_policy,
                           ContentSetting* authorize_policy);
  void OnAllowDatabase(int render_view_id,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       const string16& name,
                       const string16& display_name,
                       bool* allowed);
  void OnAllowDOMStorage(int render_view_id,
                         const GURL& origin_url,
                         const GURL& top_origin_url,
                         DOMStorageType type,
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
  void OnGetPluginContentSetting(const GURL& policy_url,
                                 const GURL& plugin_url,
                                 const std::string& resource,
                                 ContentSetting* setting,
                                 ContentSettingsPattern* primary_pattern,
                                 ContentSettingsPattern* secondary_pattern);
  void OnGetPluginInfo(int render_view_id,
                       const GURL& url,
                       const GURL& top_origin_url,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);
  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<webkit::WebPluginInfo>& plugins);
  void GetPluginInfo(int render_view_id,
                     const GURL& url,
                     const GURL& top_origin_url,
                     const std::string& mime_type,
                     ChromeViewHostMsg_GetPluginInfo_Status* status,
                     webkit::WebPluginInfo* plugin,
                     std::string* actual_mime_type);
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
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  scoped_refptr<ExtensionInfoMap> extension_info_map_;
  // Used to look up permissions at database creation time.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<CookieSettings> cookie_settings_;

  const content::ResourceContext& resource_context_;

  BooleanPrefMember allow_outdated_plugins_;
  BooleanPrefMember always_authorize_plugins_;

  base::WeakPtrFactory<ChromeRenderMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
