// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_

#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/gles2/gles2.h"
#include "mojom/native_viewport.h"
#include "ui/gfx/point_f.h"

namespace mojo {
namespace examples {

class GLES2ClientImpl {
 public:
  explicit GLES2ClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~GLES2ClientImpl();

  void HandleInputEvent(const Event& event);

 private:
  void DidCreateContext(uint32_t width, uint32_t height);
  static void DidCreateContextThunk(
      void* closure,
      uint32_t width,
      uint32_t height);
  void ContextLost();
  static void ContextLostThunk(void* closure);
  void DrawAnimationFrame();
  static void DrawAnimationFrameThunk(void* closure);

  void RequestAnimationFrames();
  void CancelAnimationFrames();

  MojoTimeTicks last_time_;
  SpinningCube cube_;
  gfx::PointF capture_point_;
  gfx::PointF last_drag_point_;
  MojoTimeTicks drag_start_time_;
  bool getting_animation_frames_;

  MojoGLES2Context context_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
