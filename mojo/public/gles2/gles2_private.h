// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_GLES2_GLES2_PRIVATE_H_
#define MOJO_PUBLIC_GLES2_GLES2_PRIVATE_H_

#include <stdint.h>

#include "mojo/public/c/system/async_waiter.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/gles2/gles2_export.h"
#include "mojo/public/gles2/gles2_types.h"

namespace mojo {
class GLES2Interface;

// Implementors of the GLES2 APIs can use this interface to install their
// implementation into the mojo_gles2 dynamic library. Mojo clients should not
// call these functions directly.
class MOJO_GLES2_EXPORT GLES2Support {
 public:
  virtual ~GLES2Support();

  static void Init(GLES2Support* gles2_support);

  virtual void Initialize(MojoAsyncWaiter* async_waiter) = 0;
  virtual void Terminate() = 0;
  virtual MojoGLES2Context CreateContext(
      MessagePipeHandle handle,
      MojoGLES2ContextLost lost_callback,
      MojoGLES2DrawAnimationFrame animation_callback,
      void* closure) = 0;
  virtual void DestroyContext(MojoGLES2Context context) = 0;
  virtual void MakeCurrent(MojoGLES2Context context) = 0;
  virtual void SwapBuffers() = 0;
  virtual void RequestAnimationFrames(MojoGLES2Context context) = 0;
  virtual void CancelAnimationFrames(MojoGLES2Context context) = 0;
  virtual void* GetGLES2Interface(MojoGLES2Context context) = 0;
  virtual void* GetContextSupport(MojoGLES2Context context) = 0;
  virtual GLES2Interface* GetGLES2InterfaceForCurrentContext() = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_GLES2_GLES2_PRIVATE_H_
