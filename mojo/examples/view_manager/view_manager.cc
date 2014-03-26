// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "mojo/examples/launcher/launcher.mojom.h"
#include "mojo/examples/view_manager/view_manager.mojom.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/shell/application.h"
#include "mojo/public/shell/shell.mojom.h"
#include "mojo/services/native_viewport/geometry_conversions.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/rect.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl)
#endif
#define VIEW_MANAGER_EXPORT __declspec(dllexport)
#else
#define CDECL
#define VIEW_MANAGER_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

class ViewImpl : public View {
 public:
  explicit ViewImpl(ScopedViewClientHandle handle)
      : id_(-1),
        client_(handle.Pass(), this) {}
  virtual ~ViewImpl() {}

 private:
  // Overridden from View:
  virtual void SetId(int view_id) OVERRIDE {
    id_ = view_id;
  }
  virtual void GetId(const mojo::Callback<void(int32_t)>& callback) OVERRIDE {
    callback.Run(id_);
  }

  int id_;
  RemotePtr<ViewClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ViewImpl);
};

class ViewManagerImpl : public Service<ViewManager, ViewManagerImpl>,
                        public NativeViewportClient,
                        public LauncherClient {
 public:
  explicit ViewManagerImpl() {
    InitNativeViewport();
  }

 private:
  // Overridden from ViewManager:
  virtual void CreateView(const Callback<void(ScopedViewHandle)>& callback)
      OVERRIDE {
    InterfacePipe<View> pipe;
    views_.push_back(new ViewImpl(pipe.handle_to_peer.Pass()));
    callback.Run(pipe.handle_to_self.Pass());
  }

  // Overridden from NativeViewportClient:
  virtual void OnCreated() OVERRIDE {
  }
  virtual void OnDestroyed() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }
  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE {
    // TODO(beng):
  }
  virtual void OnEvent(const Event& event) OVERRIDE {
    if (!event.location().is_null())
      native_viewport_->AckEvent(event);
    if (event.action() == ui::ET_KEY_RELEASED) {
      if (event.key_data().key_code() == ui::VKEY_L &&
          (event.flags() & ui::EF_CONTROL_DOWN)) {
        InitLauncher();
        launcher_->Show();
      }
    }
  }

  // Overridden from LauncherClient:
  virtual void OnURLEntered(const mojo::String& url) OVERRIDE {
    std::string url_spec = url.To<std::string>();
    printf("Received URL from launcher app: %s\n", url_spec.c_str());
    launcher_->Hide();
  }

  void InitNativeViewport() {
    InterfacePipe<NativeViewport, AnyInterface> pipe;

    AllocationScope scope;
    shell()->Connect("mojo:mojo_native_viewport_service",
                     pipe.handle_to_peer.Pass());

    native_viewport_.reset(pipe.handle_to_self.Pass(), this);
    native_viewport_->Create(gfx::Rect(50, 50, 800, 600));
    native_viewport_->Show();
  }

  void InitLauncher() {
    if (!launcher_.is_null())
      return;

    InterfacePipe<Launcher, AnyInterface> pipe;

    AllocationScope scope;
    shell()->Connect("mojo:mojo_launcher", pipe.handle_to_peer.Pass());

    launcher_.reset(pipe.handle_to_self.Pass(), this);
  }

  void DidCreateContext() {
  }

  ScopedVector<ViewImpl> views_;
  RemotePtr<NativeViewport> native_viewport_;
  RemotePtr<Launcher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerImpl);
};

}  // namespace examples
}  // namespace mojo

extern "C" VIEW_MANAGER_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;
  mojo::Application app(shell_handle);
  app.AddServiceFactory(
      new mojo::ServiceFactory<mojo::examples::ViewManagerImpl>);
  loop.Run();

  return MOJO_RESULT_OK;
}
