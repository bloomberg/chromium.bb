// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMAND_BUFFER_PROXY_H_
#define GPU_IPC_COMMAND_BUFFER_PROXY_H_

#include <string>

#include "base/callback.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/mailbox.h"

// Client side proxy that forwards messages synchronously to a
// CommandBufferStub.
class GPU_EXPORT CommandBufferProxy : public gpu::CommandBuffer {
 public:
  typedef base::Callback<void(
      const std::string& msg, int id)> GpuConsoleMessageCallback;

  CommandBufferProxy() { }

  virtual ~CommandBufferProxy() { }

  virtual int GetRouteID() const = 0;

  // Invoke the task when the channel has been flushed. Takes care of deleting
  // the task whether the echo succeeds or not.
  virtual bool Echo(const base::Closure& callback) = 0;

  // For offscreen contexts, produces the front buffer into a newly created
  // mailbox.
  virtual bool ProduceFrontBuffer(const gpu::Mailbox& mailbox) = 0;

  virtual void SetChannelErrorCallback(const base::Closure& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandBufferProxy);
};

#endif  // GPU_IPC_COMMAND_BUFFER_PROXY_H_
