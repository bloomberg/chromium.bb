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
  // Implement GLContext.
  virtual void Destroy() {}
  virtual bool MakeCurrent() { return true; }
  virtual bool IsCurrent() { return true; }
  virtual bool IsOffscreen() { return false; }
  virtual bool SwapBuffers() { return true; }
  virtual gfx::Size GetSize() { return size_; }
  virtual void* GetHandle() { return NULL; }
  virtual bool HasExtension(const char* name) { return false; }

  void SetSize(const gfx::Size& size) { size_ = size; }

 private:
  gfx::Size size_;
};

}  // namespace gfx

#endif  // APP_GFX_GL_GL_CONTEXT_STUB_H_
