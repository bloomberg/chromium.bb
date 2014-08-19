// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/gles2/command_buffer.mojom.h"
#include "mojo/services/public/interfaces/geometry/geometry.mojom.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"

namespace gfx {
class GLShareGroup;
}

namespace gpu {
namespace gles2 {
class MailboxManager;
}
}

namespace mojo {

class GpuImpl : public InterfaceImpl<Gpu> {
 public:
  GpuImpl(const scoped_refptr<gfx::GLShareGroup>& share_group,
          const scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager);

  virtual ~GpuImpl();

  virtual void CreateOnscreenGLES2Context(
      uint64_t native_viewport_id,
      SizePtr size,
      InterfaceRequest<CommandBuffer> command_buffer_request) OVERRIDE;

  virtual void CreateOffscreenGLES2Context(
      InterfaceRequest<CommandBuffer> command_buffer_request) OVERRIDE;

 private:
  // We need to share these across all NativeViewport instances so that contexts
  // they create can share resources with each other via mailboxes.
  scoped_refptr<gfx::GLShareGroup> share_group_;
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuImpl);
};

}  // namespace mojo
