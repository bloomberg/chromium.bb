// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_SURFACE_PROXY_H_
#define CONTENT_RENDERER_GPU_GPU_SURFACE_PROXY_H_
#pragma once

#if defined(ENABLE_GPU)

#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"

namespace gfx {
class Size;
}

// Client side proxy that forwards messages to a GpuSurfaceStub.
class GpuSurfaceProxy : public IPC::Channel::Listener,
                        public base::SupportsWeakPtr<GpuSurfaceProxy> {
 public:
  GpuSurfaceProxy(IPC::Channel::Sender* channel, int route_id);
  virtual ~GpuSurfaceProxy();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelError();

  int route_id() const { return route_id_; }

 private:

  // Send an IPC message over the GPU channel. This is private to fully
  // encapsulate the channel; all callers of this function must explicitly
  // verify that the channel is still available.
  bool Send(IPC::Message* msg);

  IPC::Channel::Sender* channel_;
  int route_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceProxy);
};

#endif  // ENABLE_GPU

#endif  // CONTENT_RENDERER_GPU_GPU_SURFACE_PROXY_H_
