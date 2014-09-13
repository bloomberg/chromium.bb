// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"

class Delegate : public mojo::ApplicationDelegate,
                 public mojo::InterfaceFactory<mojo::NetworkService> {
 public:
  Delegate() {}

  virtual void Initialize(mojo::ApplicationImpl* app) OVERRIDE {
    base::FilePath base_path;
    CHECK(PathService::Get(base::DIR_TEMP, &base_path));
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
    context_.reset(new mojo::NetworkContext(base_path));
  }

  // mojo::ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) OVERRIDE {
    DCHECK(context_);
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::NetworkService> implementation.
  virtual void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mojo::NetworkService> request) OVERRIDE {
    mojo::BindToRequest(
        new mojo::NetworkServiceImpl(connection, context_.get()), &request);
  }

 private:
  scoped_ptr<mojo::NetworkContext> context_;
};

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new Delegate);
  runner.set_message_loop_type(base::MessageLoop::TYPE_IO);
  return runner.Run(shell_handle);
}
