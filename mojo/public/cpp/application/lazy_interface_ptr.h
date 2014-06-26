// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LAZY_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LAZY_INTERFACE_PTR_H_

#include <string>

#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {

template<typename Interface>
class LazyInterfacePtr : InterfacePtr<Interface> {
 public:
  LazyInterfacePtr(ServiceProvider* service_provider)
      : service_provider_(service_provider) {
  }

  Interface* get() const {
    if (!InterfacePtr<Interface>::get()) {
      mojo::ConnectToService<Interface>(
          service_provider_,
          const_cast<LazyInterfacePtr<Interface>*>(this));
    }
    return InterfacePtr<Interface>::get();
  }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

 private:
  ServiceProvider* service_provider_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LAZY_INTERFACE_PTR_H_
