// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_channel_base.h"

#include <stack>

#include "base/auto_reset.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/string_number_conversions.h"
#include "chrome/common/child_process.h"
#include "ipc/ipc_sync_message.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

typedef base::hash_map<std::string, scoped_refptr<PluginChannelBase> >
    PluginChannelMap;

static PluginChannelMap g_plugin_channels_;

static base::LazyInstance<std::stack<scoped_refptr<PluginChannelBase> > >
    lazy_plugin_channel_stack_(base::LINKER_INITIALIZED);

static int next_pipe_id = 0;

PluginChannelBase* PluginChannelBase::GetChannel(
    const IPC::ChannelHandle& channel_handle, IPC::Channel::Mode mode,
    PluginChannelFactory factory, MessageLoop* ipc_message_loop,
    bool create_pipe_now) {
  scoped_refptr<PluginChannelBase> channel;
  std::string channel_key = channel_handle.name;
  PluginChannelMap::const_iterator iter = g_plugin_channels_.find(channel_key);
  if (iter == g_plugin_channels_.end()) {
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
    if (channel->Init(ipc_message_loop, create_pipe_now)) {
      g_plugin_channels_[channel_key] = channel;
    } else {
      channel = NULL;
    }
  }

  return channel;
}

void PluginChannelBase::Broadcast(IPC::Message* message) {
  for (PluginChannelMap::iterator iter = g_plugin_channels_.begin();
       iter != g_plugin_channels_.end();
       ++iter) {
    iter->second->Send(new IPC::Message(*message));
  }
  delete message;
}

PluginChannelBase::PluginChannelBase()
    : mode_(IPC::Channel::MODE_NONE),
      plugin_count_(0),
      peer_pid_(0),
      in_remove_route_(false),
      channel_valid_(false),
      in_unblock_dispatch_(0),
      send_unblocking_only_during_unblock_dispatch_(false) {
}

PluginChannelBase::~PluginChannelBase() {
}

PluginChannelBase* PluginChannelBase::GetCurrentChannel() {
  return lazy_plugin_channel_stack_.Pointer()->top();
}

void PluginChannelBase::CleanupChannels() {
  // Make a copy of the references as we can't iterate the map since items will
  // be removed from it as we clean them up.
  std::vector<scoped_refptr<PluginChannelBase> > channels;
  for (PluginChannelMap::const_iterator iter = g_plugin_channels_.begin();
       iter != g_plugin_channels_.end();
       ++iter) {
    channels.push_back(iter->second);
  }

  for (size_t i = 0; i < channels.size(); ++i)
    channels[i]->CleanUp();

  // This will clean up channels added to the map for which subsequent
  // AddRoute wasn't called
  g_plugin_channels_.clear();
}

NPObjectBase* PluginChannelBase::GetNPObjectListenerForRoute(int route_id) {
  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter == npobject_listeners_.end()) {
    DLOG(WARNING) << "Invalid route id passed in:" << route_id;
    return NULL;
  }
  return iter->second;
}

bool PluginChannelBase::Init(MessageLoop* ipc_message_loop,
                             bool create_pipe_now) {
  channel_.reset(new IPC::SyncChannel(
      channel_handle_, mode_, this, ipc_message_loop, create_pipe_now,
      ChildProcess::current()->GetShutDownEvent()));
  channel_valid_ = true;
  return true;
}

bool PluginChannelBase::Send(IPC::Message* message) {
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

int PluginChannelBase::Count() {
  return static_cast<int>(g_plugin_channels_.size());
}

bool PluginChannelBase::OnMessageReceived(const IPC::Message& message) {
  // This call might cause us to be deleted, so keep an extra reference to
  // ourself so that we can send the reply and decrement back in_dispatch_.
  lazy_plugin_channel_stack_.Pointer()->push(
      scoped_refptr<PluginChannelBase>(this));

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

  lazy_plugin_channel_stack_.Pointer()->pop();
  return handled;
}

void PluginChannelBase::OnChannelConnected(int32 peer_pid) {
  peer_pid_ = peer_pid;
}

void PluginChannelBase::AddRoute(int route_id,
                                 IPC::Channel::Listener* listener,
                                 NPObjectBase* npobject) {
  if (npobject) {
    npobject_listeners_[route_id] = npobject;
  } else {
    plugin_count_++;
  }

  router_.AddRoute(route_id, listener);
}

void PluginChannelBase::RemoveRoute(int route_id) {
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

  plugin_count_--;
  DCHECK(plugin_count_ >= 0);

  if (!plugin_count_) {
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

    for (PluginChannelMap::iterator iter = g_plugin_channels_.begin();
         iter != g_plugin_channels_.end(); ++iter) {
      if (iter->second == this) {
        g_plugin_channels_.erase(iter);
        return;
      }
    }

    NOTREACHED();
  }
}

bool PluginChannelBase::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() <<
      "should override in subclass if you care about control messages";
  return false;
}

void PluginChannelBase::OnChannelError() {
  channel_valid_ = false;
}
