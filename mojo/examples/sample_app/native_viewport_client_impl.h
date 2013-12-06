// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/examples/sample_app/gles2_client_impl.h"
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
  virtual void HandleEvent(const Event& event) MOJO_OVERRIDE;

  scoped_ptr<GLES2ClientImpl> gles2_client_;

  RemotePtr<NativeViewport> service_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(NativeViewportClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_NATIVE_VIEWPORT_CLIENT_IMPL_H_
