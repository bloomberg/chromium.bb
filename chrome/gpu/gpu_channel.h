// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_CHANNEL_H_
#define CHROME_GPU_GPU_CHANNEL_H_

#include <string>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_open_process.h"
#include "build/build_config.h"
#include "chrome/common/message_router.h"
#include "chrome/gpu/gpu_command_buffer_stub.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"

// Encapsulates an IPC channel between the GPU process and one renderer
// process. On the renderer side there's a corresponding GpuChannelHost.
class GpuChannel : public IPC::Channel::Listener,
                   public IPC::Message::Sender,
                   public base::RefCountedThreadSafe<GpuChannel> {
 public:
  // Get a new GpuChannel object for the current process to talk to the
  // given renderer process. The renderer ID is an opaque unique ID generated
  // by the browser.
  //
  // POSIX only: If |channel_fd| > 0, use that file descriptor for the
  // channel socket.
  static GpuChannel* EstablishGpuChannel(int renderer_id);

  virtual ~GpuChannel();

  std::string channel_name() const { return channel_name_; }

  base::ProcessHandle renderer_handle() const {
    return renderer_process_.handle();
  }

#if defined(OS_POSIX)
  // When first created, the GpuChannel gets assigned the file descriptor
  // for the renderer.
  // After the first time we pass it through the IPC, we don't need it anymore,
  // and we close it. At that time, we reset renderer_fd_ to -1.
  int DisownRendererFd() {
    int value = renderer_fd_;
    renderer_fd_ = -1;
    return value;
  }
#endif

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

 private:
  // Called on the plugin thread
  GpuChannel();

  bool Init(const std::string& channel_name);

  void OnControlMessageReceived(const IPC::Message& msg);

  int GenerateRouteID();

  // Message handlers.
  void OnCreateCommandBuffer(int* instance_id);
  void OnDestroyCommandBuffer(int instance_id);

  scoped_ptr<IPC::SyncChannel> channel_;
  std::string channel_name_;

  // Handle to the renderer process who is on the other side of the channel.
  base::ScopedOpenProcess renderer_process_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

#if defined(OS_POSIX)
  // FD for the renderer end of the pipe. It is stored until we send it over
  // IPC after which it is cleared. It will be closed by the IPC mechanism.
  int renderer_fd_;
#endif

  // Used to implement message routing functionality to CommandBuffer objects
  MessageRouter router_;

#if defined(ENABLE_GPU)
  typedef base::hash_map<int, scoped_refptr<GpuCommandBufferStub> > StubMap;
  StubMap stubs_;
#endif

  bool log_messages_;  // True if we should log sent and received messages.

  DISALLOW_COPY_AND_ASSIGN(GpuChannel);
};

#endif  // CHROME_GPU_GPU_CHANNEL_H_
