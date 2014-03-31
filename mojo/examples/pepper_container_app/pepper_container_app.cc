// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "mojo/examples/pepper_container_app/mojo_ppapi_globals.h"
#include "mojo/examples/pepper_container_app/plugin_instance.h"
#include "mojo/examples/pepper_container_app/plugin_module.h"
#include "mojo/examples/pepper_container_app/type_converters.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/shared_impl/proxy_lock.h"

#if defined(OS_WIN)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define PEPPER_CONTAINER_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define PEPPER_CONTAINER_APP_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class PepperContainerApp: public Application,
                          public NativeViewportClient,
                          public MojoPpapiGlobals::Delegate {
 public:
  explicit PepperContainerApp(MojoHandle shell_handle)
      : Application(shell_handle),
        ppapi_globals_(this),
        plugin_module_(new PluginModule) {
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
  }

  virtual ~PepperContainerApp() {}

  // NativeViewportClient implementation.
  virtual void OnCreated() OVERRIDE {
    ppapi::ProxyAutoLock lock;

    plugin_instance_ = plugin_module_->CreateInstance().Pass();
    if (!plugin_instance_->DidCreate())
      plugin_instance_.reset();
  }

  virtual void OnDestroyed() OVERRIDE {
    ppapi::ProxyAutoLock lock;

    if (plugin_instance_) {
      plugin_instance_->DidDestroy();
      plugin_instance_.reset();
    }

    base::MessageLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE {
    ppapi::ProxyAutoLock lock;

    if (plugin_instance_)
      plugin_instance_->DidChangeView(bounds);
  }

  virtual void OnEvent(const Event& event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    if (!event.location().is_null()) {
      ppapi::ProxyAutoLock lock;

      // TODO(yzshen): Handle events.
    }
    callback.Run();
  }

  // MojoPpapiGlobals::Delegate implementation.
  virtual ScopedMessagePipeHandle CreateGLES2Context() OVERRIDE {
    MessagePipe gles2_pipe;
    viewport_->CreateGLES2Context(gles2_pipe.handle1.Pass());
    return gles2_pipe.handle0.Pass();
  }

 private:
  MojoPpapiGlobals ppapi_globals_;

  RemotePtr<NativeViewport> viewport_;
  scoped_refptr<PluginModule> plugin_module_;
  scoped_ptr<PluginInstance> plugin_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperContainerApp);
};

}  // namespace examples
}  // namespace mojo

extern "C" PEPPER_CONTAINER_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::GLES2Initializer gles2;
  base::MessageLoop run_loop;
  mojo::examples::PepperContainerApp app(shell_handle);

  run_loop.Run();
  return MOJO_RESULT_OK;
}
