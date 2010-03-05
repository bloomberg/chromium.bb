// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_channel.h"

#include "base/command_line.h"
#include "base/lock.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_thread.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace {
class GpuReleaseTask : public Task {
 public:
  void Run() {
    ChildProcess::current()->ReleaseProcess();
  }
};

typedef base::hash_map<std::string, scoped_refptr<GpuChannel> >
    GpuChannelMap;

// How long we wait before releasing the GPU process.
const int kGpuReleaseTimeMS = 10000;

GpuChannelMap g_gpu_channels;
}  // namespace anonymous

GpuChannel* GpuChannel::EstablishGpuChannel(int renderer_id) {
  // Map renderer ID to a (single) channel to that process.
  std::string channel_name = StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), renderer_id);

  scoped_refptr<GpuChannel> channel;

  GpuChannelMap::const_iterator iter = g_gpu_channels.find(channel_name);
  if (iter == g_gpu_channels.end()) {
    channel = new GpuChannel;
  } else {
    channel = iter->second;
  }

  DCHECK(channel != NULL);

  if (!channel->channel_.get()) {
    if (channel->Init(channel_name)) {
      g_gpu_channels[channel_name] = channel;
    } else {
      channel = NULL;
    }
  }

  return channel.get();
}

GpuChannel::GpuChannel()
    : renderer_id_(-1)
#if defined(OS_POSIX)
    , renderer_fd_(-1)
#endif
{
  ChildProcess::current()->AddRefProcess();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

GpuChannel::~GpuChannel() {
#if defined(OS_POSIX)
  // If we still have the renderer FD, close it.
  if (renderer_fd_ != -1) {
    close(renderer_fd_);
  }
#endif
  ChildProcess::current()->io_message_loop()->PostDelayedTask(
      FROM_HERE,
      new GpuReleaseTask(),
      kGpuReleaseTimeMS);
}

void GpuChannel::OnChannelConnected(int32 peer_pid) {
  if (!renderer_process_.Open(peer_pid)) {
    NOTREACHED();
  }
}

void GpuChannel::OnMessageReceived(const IPC::Message& message) {
  if (log_messages_) {
    LOG(INFO) << "received message @" << &message << " on channel @" << this
              << " with type " << message.type();
  }

  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(message);
  } else {
    // The sender should know not to route messages to an object after it
    // has been destroyed.
    CHECK(router_.RouteMessage(message));
  }
}

void GpuChannel::OnChannelError() {
  // Destroy channel. This will cause the channel to be recreated if another
  // attempt is made to establish a connection from the corresponding renderer.
  channel_.reset();

  // Close renderer process handle.
  renderer_process_.Close();

#if defined(ENABLE_GPU)
  // Destroy all the stubs on this channel.
  for (size_t i = 0; i < stubs_.size(); ++i) {
    router_.RemoveRoute(stubs_[i]->route_id());
  }
  stubs_.clear();
#endif
}

bool GpuChannel::Send(IPC::Message* message) {
  if (log_messages_) {
    LOG(INFO) << "sending message @" << message << " on channel @" << this
              << " with type " << message->type();
  }

  if (!channel_.get()) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateCommandBuffer,
        OnCreateCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
        OnDestroyCommandBuffer)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

int GpuChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannel::OnCreateCommandBuffer(int* route_id) {
#if defined(ENABLE_GPU)
  *route_id = GenerateRouteID();
  scoped_refptr<GpuCommandBufferStub> stub = new GpuCommandBufferStub(
      this, *route_id);
  router_.AddRoute(*route_id, stub);
  stubs_[*route_id] = stub;
#else
  *route_id = 0;
#endif
}

void GpuChannel::OnDestroyCommandBuffer(int route_id) {
#if defined(ENABLE_GPU)
  StubMap::iterator it = stubs_.find(route_id);
  DCHECK(it != stubs_.end());
  stubs_.erase(it);
  router_.RemoveRoute(route_id);
#endif
}

bool GpuChannel::Init(const std::string& channel_name) {
  channel_name_ = channel_name;
  channel_.reset(new IPC::SyncChannel(
      channel_name, IPC::Channel::MODE_SERVER, this, NULL,
      ChildProcess::current()->io_message_loop(), false,
      ChildProcess::current()->GetShutDownEvent()));
  return true;
}
