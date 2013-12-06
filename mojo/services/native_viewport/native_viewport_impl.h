// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_

#include <string>

#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/system/core_cpp.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "mojom/native_viewport.h"

namespace gpu {
class GLInProcessContext;
}

namespace mojo {
namespace services {
class GLES2Impl;

class NativeViewportImpl : public NativeViewportStub,
                           public NativeViewportDelegate {
 public:
  NativeViewportImpl(shell::Context* context,
                     ScopedMessagePipeHandle pipe);
  virtual ~NativeViewportImpl();

  virtual void Open() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CreateGLES2Context(
      ScopedMessagePipeHandle gles2_client) OVERRIDE;

 private:
  // Overridden from services::NativeViewportDelegate:
  virtual void OnResized(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual bool OnEvent(ui::Event* ui_event) OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;

  void CreateGLES2ContextIfNeeded();

  shell::Context* context_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<services::NativeViewport> native_viewport_;
  scoped_ptr<GLES2Impl> gles2_;

  RemotePtr<NativeViewportClient> client_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportImpl);
};

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_NATIVE_VIEWPORT_IMPL_H_
