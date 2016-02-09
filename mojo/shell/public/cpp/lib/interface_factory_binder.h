// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_BINDER_H_
#define MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_BINDER_H_

#include <utility>

#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/cpp/interface_binder.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
namespace internal {

template <typename Interface>
class InterfaceFactoryBinder : public InterfaceBinder {
 public:
  explicit InterfaceFactoryBinder(InterfaceFactory<Interface>* factory)
      : factory_(factory) {}
   ~InterfaceFactoryBinder() override {}

  void BindInterface(Connection* connection,
                     const std::string& interface_name,
                     ScopedMessagePipeHandle client_handle) override {
    factory_->Create(connection,
                     MakeRequest<Interface>(std::move(client_handle)));
  }

 private:
  InterfaceFactory<Interface>* factory_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryBinder);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_LIB_INTERFACE_FACTORY_BINDER_H_
