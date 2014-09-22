// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_context.h"

namespace examples {

// ViewManagerInit is responsible for establishing the initial connection to
// the view manager. When established it loads |mojo_aura_demo|.
class ViewManagerInit : public mojo::ApplicationDelegate {
 public:
  ViewManagerInit() {}
  virtual ~ViewManagerInit() {}

  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    context_.reset(new mojo::ViewManagerContext(app));
    context_->Embed("mojo:mojo_aura_demo");
  }

 private:
  scoped_ptr<mojo::ViewManagerContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInit);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::ViewManagerInit);
  return runner.Run(shell_handle);
}
