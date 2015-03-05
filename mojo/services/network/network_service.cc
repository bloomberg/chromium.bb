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
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_connection.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/interface_factory.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"

class NetworkServiceDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::NetworkService> {
 private:
  void Initialize(mojo::ApplicationImpl* app) override {
    base::FilePath base_path;
    CHECK(PathService::Get(base::DIR_TEMP, &base_path));
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
    context_.reset(new mojo::NetworkContext(base_path));
  }

  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    DCHECK(context_);
    connection->AddService(this);
    return true;
  }

  void Quit() override {
    // Destroy the NetworkContext now as it requires MessageLoop::current() upon
    // destruction and it is the last moment we know for sure that it is
    // running.
    context_.reset();
  }

  // mojo::InterfaceFactory<mojo::NetworkService> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NetworkService> request) override {
    mojo::BindToRequest(
        new mojo::NetworkServiceImpl(connection, context_.get()), &request);
  }

 private:
  scoped_ptr<mojo::NetworkContext> context_;
};

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new NetworkServiceDelegate);
  runner.set_message_loop_type(base::MessageLoop::TYPE_IO);
  return runner.Run(shell_handle);
}
