// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/gpu/gpu_channel.h"

#include "base/command_line.h"
#include "base/lock.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_constants.h"
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

// How long we wait before releasing the GPU process.
const int kGpuReleaseTimeMS = 10000;
}  // namespace anonymous

GpuChannel::GpuChannel(int renderer_id)
    : renderer_id_(renderer_id)
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
  for (StubMap::const_iterator iter = stubs_.begin();
       iter != stubs_.end();
       ++iter) {
    router_.RemoveRoute(iter->second->route_id());
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
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateViewCommandBuffer,
        OnCreateViewCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateOffscreenCommandBuffer,
        OnCreateOffscreenCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
        OnDestroyCommandBuffer)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

int GpuChannel::GenerateRouteID() {
  static int last_id = 0;
  return ++last_id;
}

void GpuChannel::OnCreateViewCommandBuffer(gfx::NativeViewId view_id,
                                           int32* route_id) {
  *route_id = 0;

#if defined(ENABLE_GPU)

  gfx::PluginWindowHandle handle = NULL;
#if defined(OS_WIN)
  gfx::NativeView view = gfx::NativeViewFromId(view_id);

  // Check that the calling renderer is allowed to render to this window.
  // TODO(apatrick): consider killing the renderer process rather than failing.
  int view_renderer_id = reinterpret_cast<int>(
      GetProp(view, chrome::kChromiumRendererIdProperty));
  if (view_renderer_id != renderer_id_)
    return;
  handle = view;
#elif defined(OS_LINUX)
  ChildThread* gpu_thread = ChildThread::current();
  // Ask the browser for the view's XID.
  // TODO(piman): This assumes that it doesn't change. It can change however
  // when tearing off tabs. This needs a fix in the browser UI code. A possible
  // alternative would be to add a socket/plug pair like with plugins but that
  // has issues with events and focus.
  gpu_thread->Send(new GpuHostMsg_GetViewXID(view_id, &handle));
#else
  // TODO(apatrick): This needs to be something valid for mac and linux.
  // Offscreen rendering will work on these platforms but not rendering to the
  // window.
  DCHECK_EQ(view_id, 0);
#endif

  *route_id = GenerateRouteID();
  scoped_refptr<GpuCommandBufferStub> stub = new GpuCommandBufferStub(
      this, handle, NULL, gfx::Size(), 0, *route_id);
  router_.AddRoute(*route_id, stub);
  stubs_[*route_id] = stub;
#endif  // ENABLE_GPU
}

void GpuChannel::OnCreateOffscreenCommandBuffer(int32 parent_route_id,
                                                const gfx::Size& size,
                                                uint32 parent_texture_id,
                                                int32* route_id) {
#if defined(ENABLE_GPU)
  *route_id = GenerateRouteID();
  scoped_refptr<GpuCommandBufferStub> parent_stub;
  if (parent_route_id != 0) {
    StubMap::iterator it = stubs_.find(parent_route_id);
    DCHECK(it != stubs_.end());
    parent_stub = it->second;
  }

  scoped_refptr<GpuCommandBufferStub> stub = new GpuCommandBufferStub(
      this,
      NULL,
      parent_stub.get(),
      size,
      parent_texture_id,
      *route_id);
  router_.AddRoute(*route_id, stub);
  stubs_[*route_id] = stub;
#else
  *route_id = 0;
#endif
}

void GpuChannel::OnDestroyCommandBuffer(int32 route_id) {
#if defined(ENABLE_GPU)
  StubMap::iterator it = stubs_.find(route_id);
  DCHECK(it != stubs_.end());
  stubs_.erase(it);
  router_.RemoveRoute(route_id);
#endif
}

bool GpuChannel::Init() {
  // Check whether we're already initialized.
  if (channel_.get())
    return true;

  // Map renderer ID to a (single) channel to that process.
  std::string channel_name = GetChannelName();
#if defined(OS_POSIX)
  // This gets called when the GpuChannel is initially created. At this
  // point, create the socketpair and assign the GPU side FD to the channel
  // name. Keep the renderer side FD as a member variable in the PluginChannel
  // to be able to transmit it through IPC.
  int gpu_fd;
  IPC::SocketPair(&gpu_fd, &renderer_fd_);
  IPC::AddChannelSocket(channel_name, gpu_fd);
#endif
  channel_.reset(new IPC::SyncChannel(
      channel_name, IPC::Channel::MODE_SERVER, this, NULL,
      ChildProcess::current()->io_message_loop(), false,
      ChildProcess::current()->GetShutDownEvent()));
  return true;
}

std::string GpuChannel::GetChannelName() {
  return StringPrintf("%d.r%d", base::GetCurrentProcId(), renderer_id_);
}
