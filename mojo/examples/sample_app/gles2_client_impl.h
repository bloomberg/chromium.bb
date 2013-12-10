// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_

#include "base/timer/timer.h"
#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/gles2.h"

namespace mojo {
namespace examples {

class GLES2ClientImpl : public GLES2ClientStub {
 public:
  explicit GLES2ClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~GLES2ClientImpl();

 private:
  virtual void DidCreateContext(uint64_t encoded,
                                uint32_t width,
                                uint32_t height) MOJO_OVERRIDE;
  virtual void ContextLost() MOJO_OVERRIDE;

  void Draw();

  base::Time last_time_;
  base::RepeatingTimer<GLES2ClientImpl> timer_;
  SpinningCube cube_;

  RemotePtr<GLES2> service_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
