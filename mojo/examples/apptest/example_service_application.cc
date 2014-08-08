// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_service_application.h"

#include "mojo/public/cpp/application/application_connection.h"

namespace mojo {

ExampleServiceApplication::ExampleServiceApplication() {}

ExampleServiceApplication::~ExampleServiceApplication() {}

bool ExampleServiceApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(&example_service_factory_);
  return true;
}

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new ExampleServiceApplication();
}

}  // namespace mojo
