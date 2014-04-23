// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

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

class SampleApp : public Application,
                  public services::view_manager::ViewManagerClient {
 public:
  explicit SampleApp(MojoHandle shell_handle) : Application(shell_handle) {
    InterfacePipe<services::view_manager::ViewManager, AnyInterface>
        view_manager_pipe;
    AllocationScope scope;
    shell()->Connect("mojo:mojo_view_manager",
                     view_manager_pipe.handle_to_peer.Pass());
    view_manager_.reset(view_manager_pipe.handle_to_self.Pass(), this);
    view_manager_->CreateView(1, base::Bind(&SampleApp::OnCreatedView,
                                            base::Unretained(this)));
  }

  virtual ~SampleApp() {
  }

  // ViewManagerClient::
  virtual void OnConnectionEstablished(int32_t manager_id) OVERRIDE {
  }

  virtual void OnViewHierarchyChanged(
      const services::view_manager::ViewId& view,
      const services::view_manager::ViewId& new_parent,
      const services::view_manager::ViewId& old_parent,
      int32_t change_id) OVERRIDE {
  }

 private:
  void OnCreatedView(bool success) {
    DCHECK(success);
  }

  RemotePtr<services::view_manager::ViewManager> view_manager_;
};

}  // namespace examples
}  // namespace mojo

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;

  mojo::examples::SampleApp app(shell_handle);
  loop.Run();
  return MOJO_RESULT_OK;
}
