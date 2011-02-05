// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_
#define CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_
#pragma once

#if defined(ENABLE_GPU)

#include "app/surface/transport_dib.h"
#include "base/ref_counted.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"

class PluginChannel;

class CommandBufferService;

class CommandBufferStub : public IPC::Channel::Listener,
                          public IPC::Message::Sender {
 public:
  CommandBufferStub(PluginChannel* channel,
                    int plugin_host_route_id,
                    gfx::PluginWindowHandle window);

  virtual ~CommandBufferStub();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int route_id() const { return route_id_; }

  // Notify the client that it must repaint due to the window becoming invalid
  // or a lost context.
  void NotifyRepaint();

 private:
  // Message handlers:
  void OnInitialize(int32 size, base::SharedMemoryHandle* ring_buffer);
  void OnGetState(gpu::CommandBuffer::State* state);
  void OnAsyncGetState();
  void OnFlush(int32 put_offset, gpu::CommandBuffer::State* state);
  void OnAsyncFlush(int32 put_offset);
  void OnCreateTransferBuffer(int32 size, int32* id);
  void OnDestroyTransferBuffer(int32 id);
  void OnGetTransferBuffer(int32 id,
                           base::SharedMemoryHandle* transfer_buffer,
                           uint32* size);

  // Destroy all owned objects.
  void Destroy();

  bool InitializePlatformSpecific();
  void DestroyPlatformSpecific();

#if defined(OS_MACOSX)
  void OnSetWindowSize(const gfx::Size& size);
  void SwapBuffersCallback();
  void AllocTransportDIB(const size_t size, TransportDIB::Handle* dib_handle);
  void FreeTransportDIB(TransportDIB::Id dib_id);
#endif

  scoped_refptr<PluginChannel> channel_;
  int plugin_host_route_id_;
  gfx::PluginWindowHandle window_;
  int route_id_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GPUProcessor> processor_;
};

#endif  // ENABLE_GPU

#endif  // CHROME_PLUGIN_COMMAND_BUFFER_STUB_H_
