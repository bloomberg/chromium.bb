// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

namespace examples {
namespace {
void ConnectCallback(bool success) {}
}  // namespace

// This application starts the view manager, embeds the window manager and then
// starts another app (wm_flow_app) which also connects to the view manager and
// asks to be embedded without context.
class WMFlowInit : public mojo::ApplicationDelegate {
 public:
  WMFlowInit() {}
  virtual ~WMFlowInit() {}

 private:
  // Overridden from Application:
  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_view_manager", &view_manager_init_);
    mojo::ServiceProviderPtr sp;
    mojo::BindToProxy(new mojo::ServiceProviderImpl, &sp);
    view_manager_init_->Embed("mojo:mojo_wm_flow_wm", sp.Pass(),
                              base::Bind(&ConnectCallback));
    app->ConnectToApplication("mojo:mojo_wm_flow_app");
  }

  void OnConnect(bool success) {}

  mojo::ViewManagerInitServicePtr view_manager_init_;

  DISALLOW_COPY_AND_ASSIGN(WMFlowInit);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::WMFlowInit);
  return runner.Run(shell_handle);
}
