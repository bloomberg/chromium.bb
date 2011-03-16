// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_PLUGIN_CHANNEL_BASE_H_
#define CONTENT_PLUGIN_PLUGIN_CHANNEL_BASE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "content/common/message_router.h"
#include "content/plugin/npobject_base.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ui/gfx/native_widget_types.h"

// Encapsulates an IPC channel between a renderer and a plugin process.
class PluginChannelBase : public IPC::Channel::Listener,
                          public IPC::Message::Sender,
                          public base::RefCountedThreadSafe<PluginChannelBase> {
 public:

  // WebPlugin[Delegate] call these on construction and destruction to setup
  // the routing and manage lifetime of this object.  This is also called by
  // NPObjectProxy and NPObjectStub.  However the latter don't control the
  // lifetime of this object (by passing true for npobject) because we don't
  // want a leak of an NPObject in a plugin to keep the channel around longer
  // than necessary.
  void AddRoute(int route_id, IPC::Channel::Listener* listener,
                NPObjectBase* npobject);
  void RemoveRoute(int route_id);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int peer_pid() { return peer_pid_; }
  IPC::ChannelHandle channel_handle() const { return channel_handle_; }

  // Returns the number of open plugin channels in this process.
  static int Count();

  // Returns a new route id.
  virtual int GenerateRouteID() = 0;

  // Returns whether the channel is valid or not. A channel is invalid
  // if it is disconnected due to a channel error.
  bool channel_valid() {
    return channel_valid_;
  }

  // Returns the most recent PluginChannelBase to have received a message
  // in this process.
  static PluginChannelBase* GetCurrentChannel();

  static void CleanupChannels();

  // Returns the NPObjectBase object for the route id passed in.
  // Returns NULL on failure.
  NPObjectBase* GetNPObjectListenerForRoute(int route_id);

 protected:
  typedef PluginChannelBase* (*PluginChannelFactory)();

  friend class base::RefCountedThreadSafe<PluginChannelBase>;

  virtual ~PluginChannelBase();

  // Returns a PluginChannelBase derived object for the given channel name.
  // If an existing channel exists returns that object, otherwise creates a
  // new one.  Even though on creation the object is refcounted, each caller
  // must still ref count the returned value.  When there are no more routes
  // on the channel and its ref count is 0, the object deletes itself.
  static PluginChannelBase* GetChannel(
      const IPC::ChannelHandle& channel_handle, IPC::Channel::Mode mode,
      PluginChannelFactory factory, MessageLoop* ipc_message_loop,
      bool create_pipe_now);

  // Sends a message to all instances.
  static void Broadcast(IPC::Message* message);

  // Called on the worker thread
  PluginChannelBase();

  virtual void CleanUp() { }

  // Implemented by derived classes to handle control messages
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  void set_send_unblocking_only_during_unblock_dispatch() {
    send_unblocking_only_during_unblock_dispatch_ = true;
  }

  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

  scoped_ptr<IPC::SyncChannel> channel_;

 private:
  IPC::Channel::Mode mode_;
  IPC::ChannelHandle channel_handle_;
  int plugin_count_;
  int peer_pid_;

  // true when in the middle of a RemoveRoute call
  bool in_remove_route_;

  // Keep track of all the registered NPObjects proxies/stubs so that when the
  // channel is closed we can inform them.
  typedef base::hash_map<int, NPObjectBase*> ListenerMap;
  ListenerMap npobject_listeners_;

  // Used to implement message routing functionality to WebPlugin[Delegate]
  // objects
  MessageRouter router_;

  // A channel is invalid if it is disconnected as a result of a channel
  // error. This flag is used to indicate the same.
  bool channel_valid_;

  // Track whether we're dispatching a message with the unblock flag; works like
  // a refcount, 0 when we're not.
  int in_unblock_dispatch_;

  // If true, sync messages will only be marked as unblocking if the channel is
  // in the middle of dispatching an unblocking message.
  // The plugin process wants to avoid setting the unblock flag on its sync
  // messages unless necessary, since it can potentially introduce reentrancy
  // into WebKit in ways that it doesn't expect (i.e. causing layout during
  // paint).  However to avoid deadlock, we must ensure that any message that's
  // sent as a result of a sync call from the renderer must unblock the
  // renderer.  We additionally have to do this for async messages from the
  // renderer that have the unblock flag set, since they could be followed by a
  // sync message that won't get dispatched until the call to the renderer is
  // complete.
  bool send_unblocking_only_during_unblock_dispatch_;

  DISALLOW_COPY_AND_ASSIGN(PluginChannelBase);
};

#endif  // CONTENT_PLUGIN_PLUGIN_CHANNEL_BASE_H_
