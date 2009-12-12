// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_
#define CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_

#if defined(ENABLE_GPU)

#include <map>

#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

class PluginChannelHost;

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class CommandBufferProxy : public gpu::CommandBuffer,
                           public IPC::Message::Sender {
 public:
  explicit CommandBufferProxy(
      PluginChannelHost* channel,
      int route_id);
  virtual ~CommandBufferProxy();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // CommandBuffer implementation:
  virtual base::SharedMemory* Initialize(int32 size);
  virtual base::SharedMemory* GetRingBuffer();
  virtual int32 GetSize();
  virtual int32 SyncOffsets(int32 put_offset);
  virtual int32 GetGetOffset();
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 GetPutOffset();
  virtual void SetPutOffsetChangeCallback(Callback0::Type* callback);
  virtual int32 CreateTransferBuffer(size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual base::SharedMemory* GetTransferBuffer(int32 handle);
  virtual int32 GetToken();
  virtual void SetToken(int32 token);
  virtual int32 ResetParseError();
  virtual void SetParseError(int32 parse_error);
  virtual bool GetErrorStatus();
  virtual void RaiseErrorStatus();

 private:
  // As with the service, the client takes ownership of the ring buffer.
  int32 size_;
  scoped_ptr<base::SharedMemory> ring_buffer_;

  // Local cache of id to transfer buffer mapping.
  typedef std::map<int32, linked_ptr<base::SharedMemory> > TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  scoped_refptr<PluginChannelHost> channel_;
  int route_id_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxy);
};

#endif  // ENABLE_GPU

#endif  // CHROME_RENDERER_COMMAND_BUFFER_PROXY_H_
