// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/child_impl.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/string.h"

namespace mojo {
namespace examples {

class ChildApp : public ApplicationDelegate, public InterfaceFactory<Child> {
 public:
  ChildApp() {}
  virtual ~ChildApp() {}

  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    surfaces_service_connection_ =
        app->ConnectToApplication("mojo:mojo_surfaces_service");
  }

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<Child> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<Child> request) OVERRIDE {
    BindToRequest(new ChildImpl(surfaces_service_connection_), &request);
  }

 private:
  ApplicationConnection* surfaces_service_connection_;

  DISALLOW_COPY_AND_ASSIGN(ChildApp);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::ChildApp();
}

}  // namespace mojo
