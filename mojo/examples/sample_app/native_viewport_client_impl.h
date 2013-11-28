// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/examples/sample_app/sample_gles2_delegate.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/native_viewport.h"

namespace mojo {
namespace examples {

class NativeViewportClientImpl : public NativeViewportClientStub {
 public:
  explicit NativeViewportClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~NativeViewportClientImpl();

  void Open();

 private:
  virtual void DidOpen() MOJO_OVERRIDE;

  SampleGLES2Delegate gles2_delegate_;
  scoped_ptr<GLES2ClientImpl> gles2_client_;

  RemotePtr<NativeViewport> service_;
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
