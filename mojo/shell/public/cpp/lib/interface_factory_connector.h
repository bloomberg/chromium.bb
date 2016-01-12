// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_CONNECTOR_H_
#define MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_CONNECTOR_H_

#include <utility>

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/service_connector.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfaceFactoryConnector : public ServiceConnector {
 public:
  explicit InterfaceFactoryConnector(InterfaceFactory<Interface>* factory)
      : factory_(factory) {}
  ~InterfaceFactoryConnector() override {}

  void ConnectToService(ApplicationConnection* application_connection,
                        const std::string& interface_name,
                        ScopedMessagePipeHandle client_handle) override {
    factory_->Create(application_connection,
                     MakeRequest<Interface>(std::move(client_handle)));
  }

 private:
  InterfaceFactory<Interface>* factory_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryConnector);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_CONNECTOR_H_
