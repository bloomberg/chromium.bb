// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#pragma once

#include "chrome/common/content_settings.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/browser/browser_message_filter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCache.h"

class FilePath;
class Profile;

namespace net {
class URLRequestContextGetter;
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
                                        BrowserThread::ID* thread);

 private:
  friend class BrowserThread;
  friend class DeleteTask<ChromeRenderMessageFilter>;

  virtual ~ChromeRenderMessageFilter();

  void OnLaunchNaCl(const std::wstring& url,
                    int channel_descriptor,
                    IPC::Message* reply_msg);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnRendererHistograms(int sequence_number,
                            const std::vector<std::string>& histogram_info);
  void OnResourceTypeStats(const WebKit::WebCache::ResourceTypeStats& stats);
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
#if defined(USE_TCMALLOC)
  void OnRendererTcmalloc(base::ProcessId pid, const std::string& output);
#endif
  void OnGetOutdatedPluginsPolicy(ContentSetting* policy);

  int render_process_id_;

  // The Profile associated with our renderer process.  This should only be
  // accessed on the UI thread!
  Profile* profile_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  BooleanPrefMember allow_outdated_plugins_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
