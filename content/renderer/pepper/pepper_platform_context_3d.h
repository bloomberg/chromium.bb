// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "gpu/command_buffer/common/mailbox.h"

class CommandBufferProxy;
namespace gpu {
class CommandBuffer;
struct Mailbox;
}  // namespace gpu

namespace content {
class ContextProviderCommandBuffer;
class GpuChannelHost;

class PlatformContext3D {
 public:
  explicit PlatformContext3D();
  ~PlatformContext3D();

  // Initialize the context.
  bool Init(const int32* attrib_list, PlatformContext3D* share_context);

  // Retrieves the mailbox name for the front buffer backing the context.
  void GetBackingMailbox(gpu::Mailbox* mailbox, uint32* sync_point);

  // Inserts a new sync point to associate with the backing mailbox, that should
  // be waited on before using the mailbox.
  void InsertSyncPointForBackingMailbox();

  // Returns true if the backing texture is always opaque.
  bool IsOpaque();

  // This call will return the address of the command buffer for this context
  // that is constructed in Initialize() and is valid until this context is
  // destroyed.
  gpu::CommandBuffer* GetCommandBuffer();

  // Returns the GpuControl class that services out-of-band messages.
  gpu::GpuControl* GetGpuControl();

  // If the command buffer is routed in the GPU channel, return the route id.
  // Otherwise return 0.
  int GetCommandBufferRouteId();

  // Set an optional callback that will be invoked when the context is lost
  // (e.g. gpu process crash). Takes ownership of the callback.
  typedef base::Callback<void(const std::string&, int)> ConsoleMessageCallback;
  void SetContextLostCallback(const base::Closure& callback);

  // Set an optional callback that will be invoked when the GPU process
  // sends a console message.
  void SetOnConsoleMessageCallback(const ConsoleMessageCallback& callback);

  // Run the callback once the channel has been flushed.
  void Echo(const base::Closure& task);

 private:
  bool InitRaw();
  void OnContextLost();
  void OnConsoleMessage(const std::string& msg, int id);

  scoped_refptr<GpuChannelHost> channel_;
  gpu::Mailbox mailbox_;
  uint32 sync_point_;
  bool has_alpha_;
  CommandBufferProxyImpl* command_buffer_;
  base::Closure context_lost_callback_;
  ConsoleMessageCallback console_message_callback_;
  base::WeakPtrFactory<PlatformContext3D> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_CONTEXT_3D_H_
