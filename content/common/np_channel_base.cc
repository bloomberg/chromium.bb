// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/np_channel_base.h"

#include <stack>

#include "base/auto_reset.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "ipc/ipc_sync_message.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

typedef base::hash_map<std::string, scoped_refptr<NPChannelBase> > ChannelMap;
static base::LazyInstance<ChannelMap,
                          base::LeakyLazyInstanceTraits<ChannelMap> >
     g_channels = LAZY_INSTANCE_INITIALIZER;

typedef std::stack<scoped_refptr<NPChannelBase> > NPChannelRefStack;
static base::LazyInstance<NPChannelRefStack,
                          base::LeakyLazyInstanceTraits<NPChannelRefStack> >
    g_lazy_channel_stack = LAZY_INSTANCE_INITIALIZER;

static int next_pipe_id = 0;

NPChannelBase* NPChannelBase::GetChannel(
    const IPC::ChannelHandle& channel_handle, IPC::Channel::Mode mode,
    ChannelFactory factory, base::MessageLoopProxy* ipc_message_loop,
    bool create_pipe_now, base::WaitableEvent* shutdown_event) {
  scoped_refptr<NPChannelBase> channel;
  std::string channel_key = channel_handle.name;
  ChannelMap::const_iterator iter = g_channels.Get().find(channel_key);
  if (iter == g_channels.Get().end()) {
    channel = factory();
  } else {
    channel = iter->second;
  }

  DCHECK(channel != NULL);

  if (!channel->channel_valid()) {
    channel->channel_handle_ = channel_handle;
    if (mode & IPC::Channel::MODE_SERVER_FLAG) {
      channel->channel_handle_.name.append(".");
      channel->channel_handle_.name.append(base::IntToString(next_pipe_id++));
    }
    channel->mode_ = mode;
    if (channel->Init(ipc_message_loop, create_pipe_now, shutdown_event)) {
      g_channels.Get()[channel_key] = channel;
    } else {
      channel = NULL;
    }
  }

  return channel;
}

void NPChannelBase::Broadcast(IPC::Message* message) {
  for (ChannelMap::iterator iter = g_channels.Get().begin();
       iter != g_channels.Get().end();
       ++iter) {
    iter->second->Send(new IPC::Message(*message));
  }
  delete message;
}

NPChannelBase::NPChannelBase()
    : mode_(IPC::Channel::MODE_NONE),
      non_npobject_count_(0),
      peer_pid_(0),
      in_remove_route_(false),
      channel_valid_(false),
      in_unblock_dispatch_(0),
      send_unblocking_only_during_unblock_dispatch_(false) {
}

NPChannelBase::~NPChannelBase() {
}

NPChannelBase* NPChannelBase::GetCurrentChannel() {
  return g_lazy_channel_stack.Pointer()->top();
}

void NPChannelBase::CleanupChannels() {
  // Make a copy of the references as we can't iterate the map since items will
  // be removed from it as we clean them up.
  std::vector<scoped_refptr<NPChannelBase> > channels;
  for (ChannelMap::const_iterator iter = g_channels.Get().begin();
       iter != g_channels.Get().end();
       ++iter) {
    channels.push_back(iter->second);
  }

  for (size_t i = 0; i < channels.size(); ++i)
    channels[i]->CleanUp();

  // This will clean up channels added to the map for which subsequent
  // AddRoute wasn't called
  g_channels.Get().clear();
}

NPObjectBase* NPChannelBase::GetNPObjectListenerForRoute(int route_id) {
  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter == npobject_listeners_.end()) {
    DLOG(WARNING) << "Invalid route id passed in:" << route_id;
    return NULL;
  }
  return iter->second;
}

base::WaitableEvent* NPChannelBase::GetModalDialogEvent(
    gfx::NativeViewId containing_window) {
  return NULL;
}

bool NPChannelBase::Init(base::MessageLoopProxy* ipc_message_loop,
                         bool create_pipe_now,
                         base::WaitableEvent* shutdown_event) {
#if defined(OS_POSIX)
  // Check the validity of fd for bug investigation.  Remove after fixed.
  // See for details: crbug.com/95129, crbug.com/97285.
  if (mode_ == IPC::Channel::MODE_CLIENT)
    CHECK_NE(-1, channel_handle_.socket.fd);
#endif

  channel_.reset(new IPC::SyncChannel(
      channel_handle_, mode_, this, ipc_message_loop, create_pipe_now,
      shutdown_event));
  channel_valid_ = true;
  return true;
}

bool NPChannelBase::Send(IPC::Message* message) {
  if (!channel_.get()) {
    delete message;
    return false;
  }

  if (send_unblocking_only_during_unblock_dispatch_ &&
      in_unblock_dispatch_ == 0 &&
      message->is_sync()) {
    message->set_unblock(false);
  }

  return channel_->Send(message);
}

int NPChannelBase::Count() {
  return static_cast<int>(g_channels.Get().size());
}

bool NPChannelBase::OnMessageReceived(const IPC::Message& message) {
  // This call might cause us to be deleted, so keep an extra reference to
  // ourself so that we can send the reply and decrement back in_dispatch_.
  g_lazy_channel_stack.Pointer()->push(
      scoped_refptr<NPChannelBase>(this));

  bool handled;
  if (message.should_unblock())
    in_unblock_dispatch_++;
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    handled = OnControlMessageReceived(message);
  } else {
    handled = router_.RouteMessage(message);
    if (!handled && message.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      Send(reply);
    }
  }
  if (message.should_unblock())
    in_unblock_dispatch_--;

  g_lazy_channel_stack.Pointer()->pop();
  return handled;
}

void NPChannelBase::OnChannelConnected(int32 peer_pid) {
  peer_pid_ = peer_pid;
}

void NPChannelBase::AddRoute(int route_id,
                             IPC::Channel::Listener* listener,
                             NPObjectBase* npobject) {
  if (npobject) {
    npobject_listeners_[route_id] = npobject;
  } else {
    non_npobject_count_++;
  }

  router_.AddRoute(route_id, listener);
}

void NPChannelBase::RemoveRoute(int route_id) {
  router_.RemoveRoute(route_id);

  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter != npobject_listeners_.end()) {
    // This was an NPObject proxy or stub, it's not involved in the refcounting.

    // If this RemoveRoute call from the NPObject is a result of us calling
    // OnChannelError below, don't call erase() here because that'll corrupt
    // the iterator below.
    if (in_remove_route_) {
      iter->second = NULL;
    } else {
      npobject_listeners_.erase(iter);
    }

    return;
  }

  non_npobject_count_--;
  DCHECK(non_npobject_count_ >= 0);

  if (!non_npobject_count_) {
    AutoReset<bool> auto_reset_in_remove_route(&in_remove_route_, true);
    for (ListenerMap::iterator npobj_iter = npobject_listeners_.begin();
         npobj_iter != npobject_listeners_.end(); ++npobj_iter) {
      if (npobj_iter->second) {
        IPC::Channel::Listener* channel_listener =
            npobj_iter->second->GetChannelListener();
        DCHECK(channel_listener != NULL);
        channel_listener->OnChannelError();
      }
    }

    for (ChannelMap::iterator iter = g_channels.Get().begin();
         iter != g_channels.Get().end(); ++iter) {
      if (iter->second == this) {
        g_channels.Get().erase(iter);
        return;
      }
    }

    NOTREACHED();
  }
}

bool NPChannelBase::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() <<
      "should override in subclass if you care about control messages";
  return false;
}

void NPChannelBase::OnChannelError() {
  channel_valid_ = false;
}

NPObject* NPChannelBase::GetExistingNPObjectProxy(int route_id) {
  ProxyMap::iterator iter = proxy_map_.find(route_id);
  return iter != proxy_map_.end() ? iter->second : NULL;
}

int NPChannelBase::GetExistingRouteForNPObjectStub(NPObject* npobject) {
  StubMap::iterator iter = stub_map_.find(npobject);
  return iter != stub_map_.end() ? iter->second : MSG_ROUTING_NONE;
}

void NPChannelBase::AddMappingForNPObjectProxy(int route_id,
                                               NPObject* object) {
  proxy_map_[route_id] = object;
}

void NPChannelBase::AddMappingForNPObjectStub(int route_id,
                                              NPObject* object) {
  DCHECK(object != NULL);
  stub_map_[object] = route_id;
}

void NPChannelBase::RemoveMappingForNPObjectStub(int route_id,
                                                 NPObject* object) {
  DCHECK(object != NULL);
  stub_map_.erase(object);
}

void NPChannelBase::RemoveMappingForNPObjectProxy(int route_id) {
  proxy_map_.erase(route_id);
}
