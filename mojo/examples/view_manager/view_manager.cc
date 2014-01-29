// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
#include "mojo/services/native_viewport/geometry_conversions.h"
#include "mojom/launcher.h"
#include "mojom/native_viewport.h"
#include "mojom/shell.h"
#include "mojom/view_manager.h"
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
  explicit ViewImpl(ScopedMessagePipeHandle handle)
      : id_(-1),
        client_(handle.Pass()) {}
  virtual ~ViewImpl() {}

 private:
  // Overridden from View:
  virtual void SetId(int view_id) MOJO_OVERRIDE {
    id_ = view_id;
  }
  virtual void GetId() MOJO_OVERRIDE {
    client_->OnGotId(id_);
  }

  int id_;
  RemotePtr<ViewClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ViewImpl);
};

class ViewManagerImpl : public ViewManager,
                        public ShellClient,
                        public NativeViewportClient,
                        public LauncherClient {
 public:
  explicit ViewManagerImpl(ScopedMessagePipeHandle shell_handle)
      : shell_(shell_handle.Pass(), this) {
    InitNativeViewport();
  }

 private:
  // Overridden from ViewManager:
  virtual void CreateView() MOJO_OVERRIDE {
    ScopedMessagePipeHandle server_handle, client_handle;
    CreateMessagePipe(&server_handle, &client_handle);
    views_.push_back(new ViewImpl(server_handle.Pass()));
    client_->OnViewCreated(client_handle.Pass());
  }

  // Overridden from ShellClient:
  virtual void AcceptConnection(ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    client_.reset(handle.Pass(), this);
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
    AllocationScope scope;
    ScopedMessagePipeHandle client_handle, native_viewport_handle;
    CreateMessagePipe(&client_handle, &native_viewport_handle);
    shell_->Connect("mojo:mojo_native_viewport_service",
                    client_handle.Pass());
    native_viewport_.reset(native_viewport_handle.Pass(), this);

    native_viewport_->Create(gfx::Rect(50, 50, 800, 600));
    native_viewport_->Show();
  }

  void InitLauncher() {
    if (!launcher_.is_null())
      return;
    AllocationScope scope;
    ScopedMessagePipeHandle client_handle, native_viewport_handle;
    CreateMessagePipe(&client_handle, &native_viewport_handle);
    shell_->Connect("mojo:mojo_launcher", client_handle.Pass());
    launcher_.reset(native_viewport_handle.Pass(), this);
  }

  void DidCreateContext() {
  }

  RemotePtr<Shell> shell_;
  RemotePtr<ViewManagerClient> client_;
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
  mojo::examples::ViewManagerImpl view_manager(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)).Pass());
  loop.Run();

  return MOJO_RESULT_OK;
}
