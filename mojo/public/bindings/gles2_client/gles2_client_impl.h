// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_GLES2_CLIENT_GLES2_CLIENT_IMPL_H_
#define MOJO_PUBLIC_BINDINGS_GLES2_CLIENT_GLES2_CLIENT_IMPL_H_

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/gles2.h"

namespace mojo {

class GLES2Delegate {
 public:
  virtual ~GLES2Delegate();
  virtual void DidCreateContext(GLES2* gl, uint32_t width, uint32_t height);
  virtual void ContextLost(GLES2* gl);
};

class GLES2ClientImpl : public GLES2ClientStub {
 public:
  explicit GLES2ClientImpl(GLES2Delegate* delegate,
                           ScopedMessagePipeHandle gl);
  virtual ~GLES2ClientImpl();

  static void Initialize();
  static void Terminate();

  GLES2* gl() {
    return gl_.get();
  }

 private:
  virtual void DidCreateContext(
    uint64_t encoded, uint32_t width, uint32_t height) MOJO_OVERRIDE;
  virtual void ContextLost() MOJO_OVERRIDE;

  GLES2Delegate* delegate_;
  RemotePtr<GLES2> gl_;
};

}  // mojo

#endif  // MOJO_PUBLIC_BINDINGS_GLES2_CLIENT_GLES2_CLIENT_IMPL_H_
