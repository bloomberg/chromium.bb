// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_
#define CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_
#pragma once

#include "base/hash_tables.h"
#include "content/plugin/plugin_channel_base.h"
#include "ipc/ipc_channel_handle.h"

class IsListeningFilter;
class NPObjectBase;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding PluginChannel.
class PluginChannelHost : public PluginChannelBase {
 public:
  static PluginChannelHost* GetPluginChannelHost(
      const IPC::ChannelHandle& channel_handle, MessageLoop* ipc_message_loop);

  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

  virtual int GenerateRouteID();

  void AddRoute(int route_id, IPC::Channel::Listener* listener,
                NPObjectBase* npobject);
  void RemoveRoute(int route_id);

  // IPC::Channel::Listener override
  virtual void OnChannelError();

  static void SetListening(bool flag);

  static bool IsListening();

  static void Broadcast(IPC::Message* message) {
    PluginChannelBase::Broadcast(message);
  }

  bool expecting_shutdown() { return expecting_shutdown_; }

 private:
  // Called on the render thread
  PluginChannelHost();
  ~PluginChannelHost();

  static PluginChannelBase* ClassFactory() { return new PluginChannelHost(); }

  virtual bool OnControlMessageReceived(const IPC::Message& message);
  void OnSetException(const std::string& message);
  void OnPluginShuttingDown(const IPC::Message& message);

  // Keep track of all the registered WebPluginDelegeProxies to
  // inform about OnChannelError
  typedef base::hash_map<int, IPC::Channel::Listener*> ProxyMap;
  ProxyMap proxies_;

  // An IPC MessageFilter that can be told to filter out all messages. This is
  // used when the JS debugger is attached in order to avoid browser hangs.
  scoped_refptr<IsListeningFilter> is_listening_filter_;

  // True if we are expecting the plugin process to go away - in which case,
  // don't treat it as a crash.
  bool expecting_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PluginChannelHost);
};

#endif  // CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_
