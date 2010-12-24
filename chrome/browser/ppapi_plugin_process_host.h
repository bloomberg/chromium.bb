// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#define CHROME_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/browser_child_process_host.h"

class RenderMessageFilter;

class PpapiPluginProcessHost : public BrowserChildProcessHost {
 public:
  explicit PpapiPluginProcessHost(RenderMessageFilter* filter);
  virtual ~PpapiPluginProcessHost();

  void Init(const FilePath& path, IPC::Message* reply_msg);

 private:
  virtual bool CanShutdown();
  virtual void OnProcessLaunched();

  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC message handlers.
  void OnPluginLoaded(const IPC::ChannelHandle& handle);

  // Sends the reply_msg_ to the renderer with the given channel info.
  void ReplyToRenderer(base::ProcessHandle plugin_handle,
                       const IPC::ChannelHandle& channel_handle);

  RenderMessageFilter* filter_;

  // Path to the plugin library.
  FilePath plugin_path_;

  // When we're waiting for initialization of the plugin, this contains the
  // reply message for the renderer to tell it that it can continue.
  scoped_ptr<IPC::Message> reply_msg_;

  DISALLOW_COPY_AND_ASSIGN(PpapiPluginProcessHost);
};

#endif  // CHROME_BROWSER_PPAPI_PLUGIN_PROCESS_HOST_H_

