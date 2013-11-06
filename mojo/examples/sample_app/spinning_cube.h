// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_
#define MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_

#include "base/memory/scoped_ptr.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace mojo {
namespace examples {

class SpinningCube {
 public:
  SpinningCube();
  ~SpinningCube();

  void BindTo(gpu::gles2::GLES2Interface* gl,
              int width,
              int height);
  void OnGLContextLost();

  void Update(float delta_time);
  void Draw();

 private:
  class GLState;

  gpu::gles2::GLES2Interface* gl_;
  int width_;
  int height_;
  scoped_ptr<GLState> state_;
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_SAMPLE_APP_SPINNING_CUBE_H_
