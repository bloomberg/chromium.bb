// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/native_viewport/geometry_conversions.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ui/gfx/rect.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define SAMPLE_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define SAMPLE_APP_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class SampleApp : public Application, public NativeViewportClient {
 public:
  explicit SampleApp(MojoHandle shell_handle) : Application(shell_handle) {
    AllocationScope scope;

    ConnectTo("mojo:mojo_native_viewport_service", &viewport_);
    viewport_->SetClient(this);

    viewport_->Create(gfx::Rect(10, 10, 800, 600));
    viewport_->Show();

    MessagePipe gles2_pipe;
    viewport_->CreateGLES2Context(gles2_pipe.handle0.Pass());
    host_.reset(new CompositorHost(gles2_pipe.handle1.Pass()));
  }

  virtual void OnCreated() OVERRIDE {
  }

  virtual void OnDestroyed() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE {
    host_->SetSize(bounds.size());
  }

  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  NativeViewportPtr viewport_;
  scoped_ptr<CompositorHost> host_;
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;
  mojo::GLES2Initializer gles2;

  mojo::examples::SampleApp app(shell_handle);
  loop.Run();
  return MOJO_RESULT_OK;
}
