// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the StubGLContext.

#ifndef APP_GFX_GL_GL_CONTEXT_STUB_H_
#define APP_GFX_GL_GL_CONTEXT_STUB_H_
#pragma once

#include "app/gfx/gl/gl_context.h"

namespace gfx {

// A GLContext that does nothing for unit tests.
class StubGLContext : public gfx::GLContext {
 public:
  virtual ~StubGLContext();

  void SetSize(const gfx::Size& size) { size_ = size; }

  // Implement GLContext.
  virtual void Destroy() {}
  virtual bool MakeCurrent();
  virtual bool IsCurrent();
  virtual bool IsOffscreen();
  virtual bool SwapBuffers();
  virtual gfx::Size GetSize();
  virtual void* GetHandle();
  virtual void SetSwapInterval(int interval) {}
  virtual std::string GetExtensions();

 private:
  gfx::Size size_;
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_STUB_H_
