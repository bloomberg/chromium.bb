// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_CHANNEL_H_
#define CHROME_PLUGIN_PLUGIN_CHANNEL_H_
#pragma once

#include <vector>
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "build/build_config.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "chrome/plugin/webplugin_delegate_stub.h"

namespace base {
class WaitableEvent;
}

// Encapsulates an IPC channel between the plugin process and one renderer
// process.  On the renderer side there's a corresponding PluginChannelHost.
class PluginChannel : public PluginChannelBase {
 public:
  // Get a new PluginChannel object for the current process to talk to the
  // given renderer process. The renderer ID is an opaque unique ID generated
  // by the browser.
  static PluginChannel* GetPluginChannel(int renderer_id,
                                         MessageLoop* ipc_message_loop);

  // Send a message to all renderers that the process is going to shutdown.
  static void NotifyRenderersOfPendingShutdown();

  virtual ~PluginChannel();

  virtual bool Send(IPC::Message* msg);
  virtual bool OnMessageReceived(const IPC::Message& message);

  base::ProcessHandle renderer_handle() const { return renderer_handle_; }
  int renderer_id() { return renderer_id_; }

  virtual int GenerateRouteID();

  // Returns the event that's set when a call to the renderer causes a modal
  // dialog to come up.
  base::WaitableEvent* GetModalDialogEvent(gfx::NativeViewId containing_window);

  bool in_send() { return in_send_ != 0; }

  bool incognito() { return incognito_; }
  void set_incognito(bool value) { incognito_ = value; }

#if defined(OS_POSIX)
  int renderer_fd() const { return channel_->GetClientFileDescriptor(); }
#endif

 protected:
  // IPC::Channel::Listener implementation:
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  virtual void CleanUp();

  // Overrides PluginChannelBase::Init.
  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

 private:
  class MessageFilter;

  // Called on the plugin thread
  PluginChannel();

  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  static PluginChannelBase* ClassFactory() { return new PluginChannel(); }

  void OnCreateInstance(const std::string& mime_type, int* instance_id);
  void OnDestroyInstance(int instance_id, IPC::Message* reply_msg);
  void OnGenerateRouteID(int* route_id);
  void OnClearSiteData(const std::string& site,
                       uint64 flags,
                       base::Time begin_time);

  std::vector<scoped_refptr<WebPluginDelegateStub> > plugin_stubs_;

  // Handle to the renderer process who is on the other side of the channel.
  base::ProcessHandle renderer_handle_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

  int in_send_;  // Tracks if we're in a Send call.
  bool log_messages_;  // True if we should log sent and received messages.
  bool incognito_; // True if the renderer is in incognito mode.
  scoped_refptr<MessageFilter> filter_;  // Handles the modal dialog events.

  DISALLOW_COPY_AND_ASSIGN(PluginChannel);
};

#endif  // CHROME_PLUGIN_PLUGIN_CHANNEL_H_
