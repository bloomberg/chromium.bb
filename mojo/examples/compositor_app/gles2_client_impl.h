// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/gles2.h"
#include "mojom/native_viewport.h"
#include "ui/gfx/size.h"

namespace gpu {
class ContextSupport;
namespace gles2 {
class GLES2Interface;
class GLES2Implementation;
}
}

namespace mojo {
namespace examples {

class GLES2ClientImpl : public GLES2ClientStub {
 public:
  GLES2ClientImpl(
      ScopedMessagePipeHandle pipe,
      const base::Callback<void(gfx::Size)>& context_created_callback);
  virtual ~GLES2ClientImpl();

  gpu::gles2::GLES2Interface* Interface() const;
  gpu::ContextSupport* Support() const;

 private:
  virtual void DidCreateContext(uint64_t encoded,
                                uint32_t width,
                                uint32_t height) MOJO_OVERRIDE;
  virtual void ContextLost() MOJO_OVERRIDE;

  base::Callback<void(gfx::Size viewport_size)> context_created_callback_;
  gpu::gles2::GLES2Implementation* impl_;

  RemotePtr<GLES2> service_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_
