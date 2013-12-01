// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_SAMPLE_GLES2_DELEGATE_H_
#define MOJO_EXAMPLES_SAMPLE_APP_SAMPLE_GLES2_DELEGATE_H_

#include "base/timer/timer.h"
#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/bindings/gles2_client/gles2_client_impl.h"

namespace mojo {
namespace examples {

class SampleGLES2Delegate : public GLES2Delegate {
 public:
  SampleGLES2Delegate();
  virtual ~SampleGLES2Delegate();

 private:
  virtual void DidCreateContext(
      GLES2* gl, uint32_t width, uint32_t height) MOJO_OVERRIDE;
  virtual void ContextLost(GLES2* gl) MOJO_OVERRIDE;

  void Draw();

  base::Time last_time_;
  base::RepeatingTimer<SampleGLES2Delegate> timer_;
  GLES2* gl_;
  SpinningCube cube_;
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_SAMPLE_GLES2_DELEGATE_H_
