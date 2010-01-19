// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for OpenGL ES 2.0 book examples.

#ifndef GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_
#define GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_

#include "gpu/demos/app_framework/application.h"
#include "third_party/gles2_book/Common/Include/esUtil.h"

namespace gpu {
namespace demos {
namespace gles2_book {

typedef int InitFunc(ESContext* context);
typedef void UpdateFunc(ESContext* context, float elapsed_sec);
typedef void DrawFunc(ESContext* context);
typedef void ShutDownFunc(ESContext* context);

// The examples taken from OpenGL 2.0 ES book follow a well-defined pattern.
// They all use one ESContext that holds a reference to a custom UserData.
// Each example provides four functions:
// 1. InitFunc to initialize OpenGL state and custom UserData
// 2. UpdateFunc is called before drawing each frame to update animation etc.
// 3. DrawFunc is called to do the actual frame rendering
// 4. ShutDownFunc is called when program terminates
// This class encapsulates this pattern to make it easier to write classes
// for each example.
template <typename UserData,
          InitFunc init_func,
          UpdateFunc update_func,
          DrawFunc draw_func,
          ShutDownFunc shut_down_func>
class Example : public gpu_demos::Application {
 public:
  Example() {
    esInitContext(&context_);
    memset(&user_data_, 0, sizeof(UserData));
    context_.userData = &user_data_;
  }
  virtual ~Example() {
    shut_down_func(&context_);
  }

  bool Init() {
    if (!InitRenderContext()) return false;

    context_.width = width();
    context_.height = height();
    if (!init_func(&context_)) return false;
    return true;
  }

 protected:
  virtual void Draw(float elapsed_sec) {
    update_func(&context_, elapsed_sec);
    draw_func(&context_);
  }

 private:
  ESContext context_;
  UserData user_data_;
  DISALLOW_COPY_AND_ASSIGN(Example);
};

// Many examples do need to update anything and hence do not provide an
// update function. This no-op function can be used for such examples.
inline void NoOpUpdateFunc(ESContext* context, float elapsed_sec) {}

}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_
