// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

#ifdef ENABLE_GPU

namespace gpu {

class CommandBuffer;

}  // namespace gpu

class CommandBufferProxy;
class GpuChannelHost;
class RendererGLContext;
class Task;

class PlatformContext3DImpl
    : public webkit::ppapi::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl(RendererGLContext* parent_context);
  virtual ~PlatformContext3DImpl();

  virtual bool Init(const int32* attrib_list);
  virtual unsigned GetBackingTextureId();
  virtual gpu::CommandBuffer* GetCommandBuffer();
  virtual int GetCommandBufferRouteId();
  virtual void SetContextLostCallback(Callback0::Type* callback);
  virtual bool Echo(Task* task);

 private:
  bool InitRaw();
  void OnContextLost();

  base::WeakPtr<RendererGLContext> parent_context_;
  scoped_refptr<GpuChannelHost> channel_;
  unsigned int parent_texture_id_;
  CommandBufferProxy* command_buffer_;
  scoped_ptr<Callback0::Type> context_lost_callback_;
  base::ScopedCallbackFactory<PlatformContext3DImpl> callback_factory_;
};

#endif  // ENABLE_GPU

#endif  // CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
