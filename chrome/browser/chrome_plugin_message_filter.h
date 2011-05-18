// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_PLUGIN_MESSAGE_FILTER_H_
#define CHROME_BROWSER_CHROME_PLUGIN_MESSAGE_FILTER_H_
#pragma once

#include "ipc/ipc_channel_proxy.h"
#include "ui/gfx/native_widget_types.h"

class PluginProcessHost;

// This class filters out incoming Chrome-specific IPC messages for the plugin
// process on the IPC thread.
class ChromePluginMessageFilter : public IPC::ChannelProxy::MessageFilter,
                                  public IPC::Message::Sender {
 public:
  explicit ChromePluginMessageFilter(PluginProcessHost* process);

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender methods:
  virtual bool Send(IPC::Message* message);

 private:
  virtual ~ChromePluginMessageFilter();

#if defined(OS_WIN)
  void OnDownloadUrl(const std::string& url,
                     int source_child_unique_id,
                     gfx::NativeWindow caller_window);
#endif
  void OnGetPluginFinderUrl(std::string* plugin_finder_url);

  PluginProcessHost* process_;

  DISALLOW_COPY_AND_ASSIGN(ChromePluginMessageFilter);
};

#endif  // CHROME_BROWSER_CHROME_PLUGIN_MESSAGE_FILTER_H_
