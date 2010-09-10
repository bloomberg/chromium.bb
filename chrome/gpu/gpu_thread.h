// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_THREAD_H_
#define CHROME_GPU_GPU_THREAD_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/gpu_native_window_handle.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_config.h"
#include "chrome/gpu/x_util.h"
#include "gfx/native_widget_types.h"

#if defined(GPU_USE_GLX)
class GpuBackingStoreGLXContext;
#endif

class GpuThread : public ChildThread {
 public:
  GpuThread();
  ~GpuThread();

#if defined(GPU_USE_GLX)
  GpuBackingStoreGLXContext* GetGLXContext();

  Display* display() const { return display_; }
#endif

  // Remove the channel for a particular renderer.
  void RemoveChannel(int renderer_id);

 private:
  // ChildThread overrides.
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Message handlers.
  void OnEstablishChannel(int renderer_id);
  void OnSynchronize();
  void OnCollectGraphicsInfo();
  void OnNewRenderWidgetHostView(GpuNativeWindowHandle parent_window,
                                 int32 routing_id);

  typedef base::hash_map<int, scoped_refptr<GpuChannel> > GpuChannelMap;
  GpuChannelMap gpu_channels_;

#if defined(GPU_USE_GLX)
  Display* display_;
  scoped_ptr<GpuBackingStoreGLXContext> glx_context_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GpuThread);
};

#endif  // CHROME_GPU_GPU_THREAD_H_
