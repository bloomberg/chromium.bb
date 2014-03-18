// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/gles2/gles2_cpp.h"
#include "mojo/public/shell/application.h"
#include "mojo/public/shell/shell.mojom.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
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
    InterfacePipe<NativeViewport, AnyInterface> viewport_pipe;

    AllocationScope scope;
    shell()->Connect("mojo:mojo_native_viewport_service",
                     viewport_pipe.handle_to_peer.Pass());

    viewport_.reset(viewport_pipe.handle_to_self.Pass(), this);
    viewport_->Create(gfx::Rect(10, 10, 800, 600));
    viewport_->Show();
    ScopedMessagePipeHandle gles2_handle;
    ScopedMessagePipeHandle gles2_client_handle;
    CreateMessagePipe(&gles2_handle, &gles2_client_handle);

    viewport_->CreateGLES2Context(gles2_client_handle.Pass());
    host_.reset(new CompositorHost(gles2_handle.Pass()));
  }

  virtual void OnCreated() OVERRIDE {
  }

  virtual void OnDestroyed() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE {
    host_->SetSize(bounds.size());
  }

  virtual void OnEvent(const Event& event) OVERRIDE {
    if (!event.location().is_null()) {
      viewport_->AckEvent(event);
    }
  }

 private:
  RemotePtr<NativeViewport> viewport_;
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
