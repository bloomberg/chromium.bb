// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace mojo {
namespace examples {

class DemoLauncher : public ApplicationDelegate {
 public:
  DemoLauncher() {}
  virtual ~DemoLauncher() {}

 private:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_view_manager", &view_manager_init_);
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    ServiceProviderPtr sp;
    BindToProxy(new ServiceProviderImpl, &sp);
    view_manager_init_->Embed("mojo:mojo_window_manager", sp.Pass(),
                              base::Bind(&DemoLauncher::OnConnect,
                                         base::Unretained(this)));
    return true;
  }

  void OnConnect(bool success) {}

  ViewManagerInitServicePtr view_manager_init_;

  DISALLOW_COPY_AND_ASSIGN(DemoLauncher);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::DemoLauncher);
  return runner.Run(shell_handle);
}
