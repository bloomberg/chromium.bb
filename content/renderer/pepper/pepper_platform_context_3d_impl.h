// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

#ifdef ENABLE_GPU

class CommandBufferProxy;
namespace gpu {
class CommandBuffer;
}  // namespace gpu

namespace content {
class ContextProviderCommandBuffer;
class GpuChannelHost;

class PlatformContext3DImpl
    : public webkit::ppapi::PluginDelegate::PlatformContext3D {
 public:
  explicit PlatformContext3DImpl();
  virtual ~PlatformContext3DImpl();

  virtual bool Init(const int32* attrib_list,
                    PlatformContext3D* share_context) OVERRIDE;
  virtual void GetBackingMailbox(gpu::Mailbox* mailbox) OVERRIDE;
  virtual bool IsOpaque() OVERRIDE;
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual int GetCommandBufferRouteId() OVERRIDE;
  virtual void SetContextLostCallback(const base::Closure& callback) OVERRIDE;
  virtual void SetOnConsoleMessageCallback(
      const ConsoleMessageCallback& callback) OVERRIDE;
  virtual bool Echo(const base::Closure& task) OVERRIDE;

 private:
  bool InitRaw();
  void OnContextLost();
  void OnConsoleMessage(const std::string& msg, int id);

  scoped_refptr<GpuChannelHost> channel_;
  gpu::Mailbox mailbox_;
  bool has_alpha_;
  CommandBufferProxyImpl* command_buffer_;
  base::Closure context_lost_callback_;
  ConsoleMessageCallback console_message_callback_;
  base::WeakPtrFactory<PlatformContext3DImpl> weak_ptr_factory_;
};

}  // namespace content

#endif  // ENABLE_GPU

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_IMPL_H_
