// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_
#define CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_
#pragma once

#if defined(ENABLE_GPU)

#include <map>
#include <queue>

#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

namespace base {
class SharedMemory;
}

namespace gfx {
class Size;
}

class PluginChannelHost;
class Task;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxy : public gpu::CommandBuffer,
                           public IPC::Channel::Listener {
 public:
  CommandBufferProxy(IPC::Channel::Sender* channel, int route_id);
  virtual ~CommandBufferProxy();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  int route_id() const { return route_id_; }

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);
  virtual void OnSwapBuffers();

  // Set a callback that will be invoked when the SwapBuffers call has been
  // issued.
  void SetSwapBuffersCallback(Callback0::Type* callback);
  void SetChannelErrorCallback(Callback0::Type* callback);

  // Asynchronously resizes an offscreen frame buffer.
  void ResizeOffscreenFrameBuffer(const gfx::Size& size);

  // Set a task that will be invoked the next time the window becomes invalid
  // and needs to be repainted. Takes ownership of task.
  void SetNotifyRepaintTask(Task* task);

#if defined(OS_MACOSX)
  virtual void SetWindowSize(const gfx::Size& size);
#endif

  // Get the last state received from the service without synchronizing.
  State GetLastState() {
    return last_state_;
  }

  // Get the state asynchronously. The task is posted when the state is
  // updated. Takes ownership of the task object.
  void AsyncGetState(Task* completion_task);

  // Flush the command buffer asynchronously. The task is posted when the flush
  // completes. Takes ownership of the task object.
  void AsyncFlush(int32 put_offset, Task* completion_task);

 private:

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the context has not been lost.
  bool Send(IPC::Message* msg);

  // Message handlers:
  void OnUpdateState(const gpu::CommandBuffer::State& state);
  void OnNotifyRepaint();

  // As with the service, the client takes ownership of the ring buffer.
  int32 num_entries_;
  scoped_ptr<base::SharedMemory> ring_buffer_;

  // Local cache of id to transfer buffer mapping.
  typedef std::map<int32, gpu::Buffer> TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  // The last cached state received from the service.
  State last_state_;

  IPC::Channel::Sender* channel_;
  int route_id_;

  // Pending asynchronous flush callbacks.
  typedef std::queue<linked_ptr<Task> > AsyncFlushTaskQueue;
  AsyncFlushTaskQueue pending_async_flush_tasks_;

  scoped_ptr<Task> notify_repaint_task_;

  scoped_ptr<Callback0::Type> swap_buffers_callback_;
  scoped_ptr<Callback0::Type> channel_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxy);
};

#endif  // ENABLE_GPU

#endif  // CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_
