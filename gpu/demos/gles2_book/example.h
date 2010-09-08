// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Base class for OpenGL ES 2.0 book examples.

#ifndef GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_
#define GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_

#include "base/logging.h"
#include "gpu/command_buffer/common/logging.h"
#include "gpu/demos/framework/demo.h"
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
template <typename UserData>
class Example : public gpu::demos::Demo {
 public:
  Example()
    : init_func_(NULL),
      update_func_(NULL),
      draw_func_(NULL),
      shut_down_func_(NULL) {
    esInitContext(&context_);
    memset(&user_data_, 0, sizeof(UserData));
    context_.userData = &user_data_;
  }
  virtual ~Example() {
    shut_down_func_(&context_);
  }

  virtual bool InitGL() {
    // Note that update_func is optional.
    CHECK(init_func_ && draw_func_ && shut_down_func_);

    context_.width = width();
    context_.height = height();
    if (!init_func_(&context_)) return false;
    return true;
  }

 protected:
  void RegisterCallbacks(InitFunc* init_func,
                         UpdateFunc* update_func,
                         DrawFunc* draw_func,
                         ShutDownFunc* shut_down_func) {
    init_func_ = init_func;
    update_func_ = update_func;
    draw_func_ = draw_func;
    shut_down_func_ = shut_down_func;
  }

  virtual void Render(float elapsed_sec) {
    if (update_func_) update_func_(&context_, elapsed_sec);
    draw_func_(&context_);
  }

 private:
  ESContext context_;
  UserData user_data_;

  // Callback functions.
  InitFunc* init_func_;
  UpdateFunc* update_func_;
  DrawFunc* draw_func_;
  ShutDownFunc* shut_down_func_;

  DISALLOW_COPY_AND_ASSIGN(Example);
};

}  // namespace gles2_book
}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_GLES2_BOOK_EXAMPLE_H_
