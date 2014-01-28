// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "mojo/examples/sample_app/gles2_client_impl.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/environment/environment.h"
#include "mojo/public/gles2/gles2_cpp.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
#include "mojo/public/utility/run_loop.h"
#include "mojom/native_viewport.h"
#include "mojom/shell.h"

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

class SampleApp : public ShellClient {
 public:
  explicit SampleApp(ScopedMessagePipeHandle shell_handle)
      : shell_(shell_handle.Pass(), this) {

    MessagePipe pipe;
    native_viewport_client_.reset(
        new NativeViewportClientImpl(pipe.handle0.Pass()));
    mojo::AllocationScope scope;
    shell_->Connect("mojo:mojo_native_viewport_service", pipe.handle1.Pass());
  }

  virtual void AcceptConnection(ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
  }

 private:
  class NativeViewportClientImpl : public mojo::NativeViewportClient {
   public:
    explicit NativeViewportClientImpl(ScopedMessagePipeHandle viewport_handle)
        : viewport_(viewport_handle.Pass(), this) {
      AllocationScope scope;
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

      MessagePipe pipe;
      gles2_client_.reset(new GLES2ClientImpl(pipe.handle0.Pass()));
      viewport_->CreateGLES2Context(pipe.handle1.Pass());
    }

    virtual ~NativeViewportClientImpl() {}

    virtual void OnCreated() MOJO_OVERRIDE {
    }

    virtual void OnDestroyed() MOJO_OVERRIDE {
      RunLoop::current()->Quit();
    }

    virtual void OnBoundsChanged(const Rect& bounds) MOJO_OVERRIDE {
      gles2_client_->SetSize(bounds.size());
    }

    virtual void OnEvent(const Event& event) MOJO_OVERRIDE {
      if (!event.location().is_null()) {
        gles2_client_->HandleInputEvent(event);
        viewport_->AckEvent(event);
      }
    }

   private:
    scoped_ptr<GLES2ClientImpl> gles2_client_;
    RemotePtr<NativeViewport> viewport_;
  };
  mojo::RemotePtr<Shell> shell_;
  scoped_ptr<NativeViewportClientImpl> native_viewport_client_;
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;
  mojo::GLES2Initializer gles2;

  mojo::examples::SampleApp app(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)).Pass());
  loop.Run();

  return MOJO_RESULT_OK;
}
