// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "mojo/examples/sample_app/gles2_client_impl.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"

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

class SampleApp : public Application, public mojo::NativeViewportClient {
 public:
  explicit SampleApp(MojoHandle shell_handle) : Application(shell_handle) {
    InterfacePipe<NativeViewport, AnyInterface> viewport_pipe;
    mojo::AllocationScope scope;
    shell()->Connect("mojo:mojo_native_viewport_service",
                     viewport_pipe.handle_to_peer.Pass());
    viewport_.reset(viewport_pipe.handle_to_self.Pass(), this);
    Rect::Builder rect;
    Point::Builder point;
    point.set_x(10);
    point.set_y(10);
    rect.set_position(point.Finish());
    Size::Builder size;
    size.set_width(800);
    size.set_height(600);
    rect.set_size(size.Finish());
    viewport_->Create(rect.Finish());
    viewport_->Show();

    MessagePipe gles2_pipe;
    viewport_->CreateGLES2Context(gles2_pipe.handle1.Pass());
    gles2_client_.reset(new GLES2ClientImpl(gles2_pipe.handle0.Pass()));
  }

  virtual ~SampleApp() {
    // TODO(darin): Fix shutdown so we don't need to leak this.
    MOJO_ALLOW_UNUSED GLES2ClientImpl* leaked = gles2_client_.release();
  }

  virtual void OnCreated() MOJO_OVERRIDE {
  }

  virtual void OnDestroyed() MOJO_OVERRIDE {
    RunLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) MOJO_OVERRIDE {
    gles2_client_->SetSize(bounds.size());
  }

  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) MOJO_OVERRIDE {
    if (!event.location().is_null())
      gles2_client_->HandleInputEvent(event);
    callback.Run();
  }

 private:
  scoped_ptr<GLES2ClientImpl> gles2_client_;
  RemotePtr<NativeViewport> viewport_;
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;
  mojo::GLES2Initializer gles2;

  mojo::examples::SampleApp app(shell_handle);
  loop.Run();
  return MOJO_RESULT_OK;
}
