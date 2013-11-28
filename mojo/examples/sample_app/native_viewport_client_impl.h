// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/native_viewport.h"

namespace mojo {
namespace examples {

class NativeViewportClientImpl : public NativeViewportClientStub {
 public:
  explicit NativeViewportClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~NativeViewportClientImpl();

  virtual void DidOpen() MOJO_OVERRIDE;
  virtual void DidCreateGLContext(uint64_t gl) MOJO_OVERRIDE;

  NativeViewport* service() {
    return service_.get();
  }

 private:
  RemotePtr<NativeViewport> service_;
};

}  // examples
}  // mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
