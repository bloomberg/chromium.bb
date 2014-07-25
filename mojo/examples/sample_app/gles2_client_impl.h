// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_

#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/services/public/interfaces/geometry/geometry.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"

namespace examples {

class GLES2ClientImpl {
 public:
  explicit GLES2ClientImpl(mojo::CommandBufferPtr command_buffer);
  virtual ~GLES2ClientImpl();

  void SetSize(const mojo::Size& size);
  void HandleInputEvent(const mojo::Event& event);
  void Draw();

 private:
  void ContextLost();
  static void ContextLostThunk(void* closure);
  void WantToDraw();

  MojoTimeTicks last_time_;
  mojo::Size size_;
  SpinningCube cube_;
  mojo::Point capture_point_;
  mojo::Point last_drag_point_;
  MojoTimeTicks drag_start_time_;
  bool waiting_to_draw_;

  MojoGLES2Context context_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples

#endif  // MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
