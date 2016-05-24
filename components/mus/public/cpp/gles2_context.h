// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_GLES2_CONTEXT_H_
#define COMPONENTS_MUS_PUBLIC_CPP_GLES2_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/lib/command_buffer_client_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "mojo/public/c/gles2/gles2.h"

struct MojoGLES2ContextPrivate {};

namespace gpu {
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}
}

namespace mus {

class GLES2Context : public MojoGLES2ContextPrivate {
 public:
  explicit GLES2Context(const std::vector<int32_t>& attribs,
                        mojo::ScopedMessagePipeHandle command_buffer_handle,
                        MojoGLES2ContextLost lost_callback,
                        void* closure);
  virtual ~GLES2Context();
  bool Initialize();

  gpu::gles2::GLES2Interface* interface() const {
    return implementation_.get();
  }
  gpu::ContextSupport* context_support() const { return implementation_.get(); }

 private:
  void OnLostContext();

  CommandBufferClientImpl command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> implementation_;
  MojoGLES2ContextLost lost_callback_;
  void* closure_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Context);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_GLES2_CONTEXT_H_
