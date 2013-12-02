// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_GLES2_GLES2_IMPL_H_
#define MOJO_SERVICES_GLES2_GLES2_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/system/core_cpp.h"
#include "mojom/gles2.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gpu {
class GLInProcessContext;
}

namespace mojo {
namespace services {

class GLES2Impl : public GLES2Stub {
 public:
  explicit GLES2Impl(ScopedMessagePipeHandle client);
  virtual ~GLES2Impl();

  virtual void Destroy() OVERRIDE;

  void CreateContext(gfx::AcceleratedWidget widget, const gfx::Size& size);

 private:
  void OnGLContextLost();

  scoped_ptr<gpu::GLInProcessContext> gl_context_;
  RemotePtr<GLES2Client> client_;

  DISALLOW_COPY_AND_ASSIGN(GLES2Impl);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_GLES2_GLES2_IMPL_H_
