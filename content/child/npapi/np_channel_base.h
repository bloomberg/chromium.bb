// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_NP_CHANNEL_BASE_H_
#define CONTENT_CHILD_NPAPI_NP_CHANNEL_BASE_H_

#include <stdint.h>

#include <string>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/message_router.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// Encapsulates an IPC channel between a renderer and another process. Used to
// proxy access to NP objects.
class NPChannelBase : public IPC::Listener,
                      public IPC::Sender,
                      public base::RefCountedThreadSafe<NPChannelBase> {
 public:
  // WebPlugin[Delegate] call these on construction and destruction to setup
  // the routing and manage lifetime of this object.
  void AddRoute(int route_id, IPC::Listener* listener);
  void RemoveRoute(int route_id);

  // IPC::Sender implementation:
  bool Send(IPC::Message* msg) override;

  base::ProcessId peer_pid() { return channel_->GetPeerPID(); }
  IPC::ChannelHandle channel_handle() const { return channel_handle_; }

  // Returns a new route id.
  virtual int GenerateRouteID() = 0;

  // Returns whether the channel is valid or not. A channel is invalid
  // if it is disconnected due to a channel error.
  bool channel_valid() {
    return channel_valid_;
  }

  // Returns the most recent NPChannelBase to have received a message
  // in this process.
  static NPChannelBase* GetCurrentChannel();

  static void CleanupChannels();

  // Returns the event that's set when a call to the renderer causes a modal
  // dialog to come up. The default implementation returns NULL. Derived
  // classes should override this method if this functionality is required.
  virtual base::WaitableEvent* GetModalDialogEvent(int render_view_id);

 protected:
  typedef NPChannelBase* (*ChannelFactory)();

  friend class base::RefCountedThreadSafe<NPChannelBase>;

  ~NPChannelBase() override;

  // Returns a NPChannelBase derived object for the given channel name.
  // If an existing channel exists returns that object, otherwise creates a
  // new one.  Even though on creation the object is refcounted, each caller
  // must still ref count the returned value.  When there are no more routes
  // on the channel and its ref count is 0, the object deletes itself.
  static NPChannelBase* GetChannel(
      const IPC::ChannelHandle& channel_handle,
      IPC::Channel::Mode mode,
      ChannelFactory factory,
      base::SingleThreadTaskRunner* ipc_task_runner,
      bool create_pipe_now,
      base::WaitableEvent* shutdown_event);

  // Sends a message to all instances.
  static void Broadcast(IPC::Message* message);

  // Called on the worker thread
  NPChannelBase();

  virtual void CleanUp() { }

  // Implemented by derived classes to handle control messages
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Listener implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  void set_send_unblocking_only_during_unblock_dispatch() {
    send_unblocking_only_during_unblock_dispatch_ = true;
  }

  virtual bool Init(base::SingleThreadTaskRunner* ipc_task_runner,
                    bool create_pipe_now,
                    base::WaitableEvent* shutdown_event);

  scoped_ptr<IPC::SyncChannel> channel_;
  IPC::ChannelHandle channel_handle_;

 private:
  IPC::Channel::Mode mode_;
  // This tracks the number of routes registered without an NPObject. It's used
  // to manage the lifetime of this object. See comment for AddRoute() and
  // RemoveRoute().
  int non_npobject_count_;
  int peer_pid_;

  // true when in the middle of a RemoveRoute call
  bool in_remove_route_;

  // Used to implement message routing functionality to WebPlugin[Delegate]
  // objects
  IPC::MessageRouter router_;

  // A channel is invalid if it is disconnected as a result of a channel
  // error. This flag is used to indicate the same.
  bool channel_valid_;

  // Track whether we're dispatching a message with the unblock flag; works like
  // a refcount, 0 when we're not.
  int in_unblock_dispatch_;

  // If true, sync messages will only be marked as unblocking if the channel is
  // in the middle of dispatching an unblocking message.  The non-renderer
  // process wants to avoid setting the unblock flag on its sync messages
  // unless necessary, since it can potentially introduce reentrancy into
  // WebKit in ways that it doesn't expect (i.e. causing layout during paint).
  // However to avoid deadlock, we must ensure that any message that's sent as
  // a result of a sync call from the renderer must unblock the renderer.  We
  // additionally have to do this for async messages from the renderer that
  // have the unblock flag set, since they could be followed by a sync message
  // that won't get dispatched until the call to the renderer is complete.
  bool send_unblocking_only_during_unblock_dispatch_;

  DISALLOW_COPY_AND_ASSIGN(NPChannelBase);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_NP_CHANNEL_BASE_H_
