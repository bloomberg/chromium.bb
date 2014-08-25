// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_service_application.h"

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_runner.h"

namespace mojo {

ExampleServiceApplication::ExampleServiceApplication() {}

ExampleServiceApplication::~ExampleServiceApplication() {}

bool ExampleServiceApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&example_service_factory_);
  return true;
}

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::ExampleServiceApplication());
  return runner.Run(shell_handle);
}
