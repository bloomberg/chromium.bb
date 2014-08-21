// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/services/view_manager/view_manager_init_service_context.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace service {

class ViewManagerApp : public ApplicationDelegate,
                       public InterfaceFactory<ViewManagerInitService> {
 public:
  ViewManagerApp() {}
  virtual ~ViewManagerApp() {}

  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    context_.ConfigureIncomingConnection(connection);
    // TODO(sky): this needs some sort of authentication as well as making sure
    // we only ever have one active at a time.
    connection->AddService(this);
    return true;
  }

  virtual void Create(
      ApplicationConnection* connection,
      InterfaceRequest<ViewManagerInitService> request) OVERRIDE {
    BindToRequest(new ViewManagerInitServiceImpl(connection, &context_),
                  &request);
  }

 private:
  ViewManagerInitServiceContext context_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerApp);
};

}  // namespace service
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::service::ViewManagerApp);
  return runner.Run(shell_handle);
}
