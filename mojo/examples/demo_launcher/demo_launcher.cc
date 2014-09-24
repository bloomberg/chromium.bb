// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_context.h"

namespace examples {

class DemoLauncher : public mojo::ApplicationDelegate {
 public:
  DemoLauncher() {}
  virtual ~DemoLauncher() {}

 private:
  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    context_.reset(new mojo::ViewManagerContext(app));
    context_->Embed("mojo:mojo_window_manager");
  }

  scoped_ptr<mojo::ViewManagerContext> context_;

  DISALLOW_COPY_AND_ASSIGN(DemoLauncher);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::DemoLauncher);
  return runner.Run(shell_handle);
}
