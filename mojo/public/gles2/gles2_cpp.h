// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_GLES2_CPP_H_
#define MOJO_PUBLIC_GLES2_GLES2_CPP_H_

#include "mojo/public/environment/default_async_waiter.h"
#include "mojo/public/gles2/gles2.h"

namespace mojo {

class GLES2Initializer {
 public:
  GLES2Initializer(MojoAsyncWaiter* async_waiter = GetDefaultAsyncWaiter()) {
    MojoGLES2Initialize(async_waiter);
  }
  ~GLES2Initializer() { MojoGLES2Terminate(); }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_GLES2_GLES2_H_
