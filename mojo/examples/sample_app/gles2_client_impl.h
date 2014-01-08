// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_

#include "mojo/examples/sample_app/spinning_cube.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/gles2.h"
#include "mojom/native_viewport.h"
#include "ui/gfx/point_f.h"

namespace mojo {
namespace examples {

class GLES2ClientImpl : public GLES2Client {
 public:
  explicit GLES2ClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~GLES2ClientImpl();

  void HandleInputEvent(const Event& event);

 private:
  virtual void DidCreateContext(uint64_t encoded,
                                uint32_t width,
                                uint32_t height) MOJO_OVERRIDE;
  virtual void ContextLost() MOJO_OVERRIDE;
  virtual void DrawAnimationFrame() MOJO_OVERRIDE;

  void RequestAnimationFrames();
  void CancelAnimationFrames();

  MojoTimeTicks last_time_;
  SpinningCube cube_;
  gfx::PointF capture_point_;
  gfx::PointF last_drag_point_;
  MojoTimeTicks drag_start_time_;
  bool getting_animation_frames_;

  RemotePtr<GLES2> service_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_GLES2_CLIENT_IMPL_H_
