// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

#ifdef ENABLE_GPU

namespace gpu {

class CommandBuffer;

}  // namespace gpu

class CommandBufferProxy;
class GpuChannelHost;
class PepperParentContextProvider;
class RendererGLContext;

class PlatformContext3DImpl
    : public webkit::ppapi::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl(
      PepperParentContextProvider* parent_context_provider);
  virtual ~PlatformContext3DImpl();

  virtual bool Init(const int32* attrib_list) OVERRIDE;
  virtual unsigned GetBackingTextureId() OVERRIDE;
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual int GetCommandBufferRouteId() OVERRIDE;
  virtual void SetContextLostCallback(const base::Closure& callback) OVERRIDE;
  virtual bool Echo(const base::Closure& task) OVERRIDE;

 private:
  bool InitRaw();
  void OnContextLost();

  // Implicitly weak pointer; must outlive this instance.
  PepperParentContextProvider* parent_context_provider_;
  base::WeakPtr<RendererGLContext> parent_context_;
  scoped_refptr<GpuChannelHost> channel_;
  unsigned int parent_texture_id_;
  CommandBufferProxy* command_buffer_;
  base::Closure context_lost_callback_;
  base::WeakPtrFactory<PlatformContext3DImpl> weak_ptr_factory_;
};

#endif  // ENABLE_GPU

#endif  // CONTENT_RENDERER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
