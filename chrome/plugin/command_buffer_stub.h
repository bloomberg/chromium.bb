// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_
#define CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_

#if defined(ENABLE_GPU)

#include "app/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

class PluginChannel;

class CommandBufferService;

class CommandBufferStub : public IPC::Channel::Listener,
                          public IPC::Message::Sender {
 public:
  CommandBufferStub(PluginChannel* channel, gfx::PluginWindowHandle window);

  virtual ~CommandBufferStub();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int route_id() const { return route_id_; }

 private:
  // Message handlers:
  void OnInitialize(int32 size, base::SharedMemoryHandle* ring_buffer);
  void OnSyncOffsets(int32 put_offset, int32* get_offset);
  void OnGetGetOffset(int32* get_offset);
  void OnGetPutOffset(int32* put_offset);
  void OnCreateTransferBuffer(int32 size, int32* id);
  void OnDestroyTransferBuffer(int32 id);
  void OnGetTransferBuffer(int32 id,
                           base::SharedMemoryHandle* transfer_buffer,
                           size_t* size);
  void OnGetToken(int32* token);
  void OnResetParseError(int32* parse_error);
  void OnGetErrorStatus(bool* error_status);

  scoped_refptr<PluginChannel> channel_;
  gfx::PluginWindowHandle window_;
  int route_id_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_refptr<gpu::GPUProcessor> processor_;
};

#endif  // ENABLE_GPU

#endif  // CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_
