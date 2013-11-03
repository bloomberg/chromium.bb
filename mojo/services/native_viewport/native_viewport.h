// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_

#include "base/memory/scoped_ptr.h"

namespace gfx {
class Size;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace ui {
class Event;
}

namespace mojo {
namespace shell {
class Context;
}

namespace services {

class NativeViewportDelegate {
 public:
  virtual ~NativeViewportDelegate() {}

  virtual bool OnEvent(ui::Event* event) = 0;
  virtual void OnDestroyed() = 0;
  virtual void OnGLContextAvailable(gpu::gles2::GLES2Interface* gl) = 0;
  virtual void OnGLContextLost() = 0;
  virtual void OnResized(const gfx::Size& size) = 0;
};

// Encapsulation of platform-specific Viewport.
class NativeViewport {
 public:
  virtual ~NativeViewport() {}

  virtual void Close() = 0;

  static scoped_ptr<NativeViewport> Create(shell::Context* context,
                                           NativeViewportDelegate* delegate);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_H_
