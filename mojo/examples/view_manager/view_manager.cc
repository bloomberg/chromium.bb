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

// Convenience interface to connect to a service. Necessary because all code for
// this app lives in one .cc file.
class Connector {
 public:
  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_handle) = 0;

 protected:
  virtual ~Connector() {}
};

class NativeViewportClientImpl : public NativeViewportClient,
                                 public LauncherClient {
 public:
  explicit NativeViewportClientImpl(Connector* connector)
      : connector_(connector) {
    AllocationScope scope;
    ScopedMessagePipeHandle client_handle, native_viewport_handle;
    CreateMessagePipe(&client_handle, &native_viewport_handle);
    connector_->Connect("mojo:mojo_native_viewport_service",
                        client_handle.Pass());
    native_viewport_.reset(native_viewport_handle.Pass(), this);

    native_viewport_->Create(gfx::Rect(50, 50, 800, 600));
    native_viewport_->Show();
  }
  virtual ~NativeViewportClientImpl() {}

 private:
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

  void InitLauncher() {
    if (!launcher_.is_null())
      return;
    AllocationScope scope;
    ScopedMessagePipeHandle client_handle, native_viewport_handle;
    CreateMessagePipe(&client_handle, &native_viewport_handle);
    connector_->Connect("mojo:mojo_launcher", client_handle.Pass());
    launcher_.reset(native_viewport_handle.Pass(), this);
  }

  Connector* connector_;
  RemotePtr<NativeViewport> native_viewport_;
  RemotePtr<Launcher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportClientImpl);
};

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

class ViewManagerImpl : public ViewManager {
 public:
  explicit ViewManagerImpl(ScopedMessagePipeHandle handle)
      : client_(handle.Pass()) {
  }
  virtual ~ViewManagerImpl() {}

 private:
  // Overridden from ViewManager:
  virtual void CreateView() MOJO_OVERRIDE {
    ScopedMessagePipeHandle server_handle, client_handle;
    CreateMessagePipe(&server_handle, &client_handle);
    views_.push_back(new ViewImpl(server_handle.Pass()));
    client_->OnViewCreated(client_handle.Pass());
  }

  RemotePtr<ViewManagerClient> client_;
  ScopedVector<ViewImpl> views_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerImpl);
};

class ViewManager : public ShellClient,
                    public Connector {
 public:
  explicit ViewManager(ScopedMessagePipeHandle shell_handle)
      : shell_(shell_handle.Pass(), this) {
    nvc_.reset(new NativeViewportClientImpl(this));
  }

 private:
  // Overridden from ShellClient:
  virtual void AcceptConnection(ScopedMessagePipeHandle handle) MOJO_OVERRIDE {
    view_managers_.push_back(new ViewManagerImpl(handle.Pass()));
  }

  // Overridden from Connector:
  virtual void Connect(const String& url,
                       ScopedMessagePipeHandle client_handle) OVERRIDE {
    shell_->Connect(url, client_handle.Pass());
  }

  RemotePtr<Shell> shell_;
  scoped_ptr<NativeViewportClientImpl> nvc_;
  ScopedVector<ViewManagerImpl> view_managers_;

  DISALLOW_COPY_AND_ASSIGN(ViewManager);
};

}  // namespace examples
}  // namespace mojo

extern "C" VIEW_MANAGER_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::MessageLoop loop;
  mojo::examples::ViewManager view_manager(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)).Pass());
  loop.Run();

  return MOJO_RESULT_OK;
}
