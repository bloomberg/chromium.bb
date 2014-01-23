// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/message_loop/message_loop.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/examples/compositor_app/gles2_client_impl.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/public/gles2/gles2_cpp.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
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
    mojo::ScopedMessagePipeHandle client_handle, native_viewport_handle;
    CreateMessagePipe(&client_handle, &native_viewport_handle);
    native_viewport_client_.reset(
        new NativeViewportClientImpl(native_viewport_handle.Pass()));
    mojo::AllocationScope scope;
    shell_->Connect("mojo:mojo_native_viewport_service", client_handle.Pass());
  }

  virtual void AcceptConnection(ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    NOTREACHED() << "SampleApp can't be connected to.";
  }

 private:
  class NativeViewportClientImpl : public mojo::NativeViewportClient {
   public:
    explicit NativeViewportClientImpl(ScopedMessagePipeHandle viewport_handle)
        : viewport_(viewport_handle.Pass(), this) {
      viewport_->Open();
      ScopedMessagePipeHandle gles2_handle;
      ScopedMessagePipeHandle gles2_client_handle;
      CreateMessagePipe(&gles2_handle, &gles2_client_handle);

      gles2_client_.reset(new GLES2ClientImpl(
            gles2_handle.Pass(),
            base::Bind(&NativeViewportClientImpl::DidCreateContext,
                       base::Unretained(this))));
      viewport_->CreateGLES2Context(gles2_client_handle.Pass());
      host_.reset(new CompositorHost(gles2_client_.get()));
    }

    void DidCreateContext(gfx::Size viewport_size) {
      host_->DidCreateContext(viewport_size);
    }

    virtual ~NativeViewportClientImpl() {}

    virtual void OnCreated() MOJO_OVERRIDE {
    }

    virtual void OnDestroyed() MOJO_OVERRIDE {
      base::MessageLoop::current()->Quit();
    }

    virtual void OnEvent(const Event& event) MOJO_OVERRIDE {
      if (!event.location().is_null()) {
        viewport_->AckEvent(event);
      }
    }

   private:
    scoped_ptr<GLES2ClientImpl> gles2_client_;
    RemotePtr<NativeViewport> viewport_;
    scoped_ptr<CompositorHost> host_;
  };
  mojo::RemotePtr<Shell> shell_;
  scoped_ptr<NativeViewportClientImpl> native_viewport_client_;
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;
  mojo::GLES2Initializer gles2;

  mojo::examples::SampleApp app(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)).Pass());
  loop.Run();

  return MOJO_RESULT_OK;
}
