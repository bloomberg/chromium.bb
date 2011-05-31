// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_SURFACE_STUB_H_
#define CONTENT_COMMON_GPU_GPU_SURFACE_STUB_H_
#pragma once

#if defined(ENABLE_GPU)

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/gl/gl_surface.h"

class GpuChannel;

class GpuSurfaceStub
    : public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public base::SupportsWeakPtr<GpuSurfaceStub> {
 public:
  // Takes ownership of surface.
  GpuSurfaceStub(GpuChannel* channel, int route_id, gfx::GLSurface* surface);
  virtual ~GpuSurfaceStub();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

 private:
  // Message handlers.
  // None yet.

  // This is a weak pointer. The GpuChannel controls the lifetime of the
  // GpuSurfaceStub and always outlives it.
  GpuChannel* channel_;

  int route_id_;
  scoped_ptr<gfx::GLSurface> surface_;
  DISALLOW_COPY_AND_ASSIGN(GpuSurfaceStub);
};

#endif  // defined(ENABLE_GPU)

#endif  // CONTENT_COMMON_GPU_GPU_SURFACE_STUB_H_
