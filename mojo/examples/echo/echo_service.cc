// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/echo/echo_service.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"

namespace mojo {
namespace examples {

class EchoServiceImpl : public InterfaceImpl<EchoService> {
 public:
  virtual void EchoString(
      const String& value,
      const Callback<void(String)>& callback) MOJO_OVERRIDE {
    callback.Run(value);
  }
};

class EchoServiceDelegate : public ApplicationDelegate {
 public:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&echo_service_factory_);
    return true;
  }

 private:
  InterfaceFactoryImpl<EchoServiceImpl> echo_service_factory_;
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::EchoServiceDelegate();
}

}  // namespace mojo
