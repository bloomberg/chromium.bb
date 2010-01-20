// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_
#define CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_

#include "base/hash_tables.h"
#include "chrome/plugin/plugin_channel_base.h"

class IsListeningFilter;
class NPObjectBase;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding PluginChannel.
class PluginChannelHost : public PluginChannelBase {
 public:
  static PluginChannelHost* GetPluginChannelHost(
      const std::string& channel_name, MessageLoop* ipc_message_loop);

  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

  int GenerateRouteID();

  void AddRoute(int route_id, IPC::Channel::Listener* listener,
                NPObjectBase* npobject);
  void RemoveRoute(int route_id);

  // IPC::Channel::Listener override
  void OnChannelError();

  static void SetListening(bool flag);

  static bool IsListening();

  static void Broadcast(IPC::Message* message) {
    PluginChannelBase::Broadcast(message);
  }

 private:
  // Called on the render thread
  PluginChannelHost();
  ~PluginChannelHost();

  static PluginChannelBase* ClassFactory() { return new PluginChannelHost(); }

  void OnControlMessageReceived(const IPC::Message& message);
  void OnSetException(const std::string& message);

  // Keep track of all the registered WebPluginDelegeProxies to
  // inform about OnChannelError
  typedef base::hash_map<int, IPC::Channel::Listener*> ProxyMap;
  ProxyMap proxies_;

  // An IPC MessageFilter that can be told to filter out all messages. This is
  // used when the JS debugger is attached in order to avoid browser hangs.
  scoped_refptr<IsListeningFilter> is_listening_filter_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginChannelHost);
};

#endif  // CHROME_RENDERER_PLUGIN_CHANNEL_HOST_H_
