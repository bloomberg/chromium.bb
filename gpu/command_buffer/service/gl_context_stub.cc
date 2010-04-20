// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the StubGLContext.

#include "build/build_config.h"
#include "gpu/command_buffer/service/gl_context.h"

namespace gpu {

// A GLContext that does nothing for unit tests.
class StubGLContext : public GLContext {
 public:

  // Implement GLContext.
  virtual void Destroy() {}
  virtual bool MakeCurrent() { return true; }
  virtual bool IsCurrent() { return true; }
  virtual bool IsOffscreen() { return true; }
  virtual void SwapBuffers() {}
  virtual gfx::Size GetSize() { return gfx::Size(); }
  virtual void* GetHandle() { return NULL; }
};

#if !defined(OS_MACOSX)

GLContext* GLContext::CreateViewGLContext(gfx::PluginWindowHandle /* window */,
                                          bool /* multisampled */) {
  return new StubGLContext;
}

#endif  // OS_MACOSX

GLContext* GLContext::CreateOffscreenGLContext(void* /* shared_handle */) {
  return new StubGLContext;
}

}  // namespace gpu
