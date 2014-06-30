// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SERVICE_REGISTRY_H_
#define CONTENT_PUBLIC_COMMON_SERVICE_REGISTRY_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/core.h"

namespace content {

// A ServiceRegistry exposes local services that have been added using
// AddService to a paired remote ServiceRegistry and provides local access to
// services exposed by the remote ServiceRegistry through
// ConnectToRemoteService.
class CONTENT_EXPORT ServiceRegistry {
 public:
  virtual ~ServiceRegistry() {}

  // Make the service created by |service_factory| available to the remote
  // ServiceProvider. In response to each request for a service,
  // |service_factory| will be run with an InterfaceRequest<Interface>
  // representing that request.
  template <typename Interface>
  void AddService(const base::Callback<void(mojo::InterfaceRequest<Interface>)>
                      service_factory) {
    AddService(Interface::Name_,
               base::Bind(&ServiceRegistry::ForwardToServiceFactory<Interface>,
                          service_factory));
  }
  virtual void AddService(
      const std::string& service_name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>
          service_factory) = 0;

  // Remove future access to the service implementing Interface. Existing
  // connections to the service are unaffected.
  template <typename Interface>
  void RemoveService() {
    RemoveService(Interface::Name_);
  }
  virtual void RemoveService(const std::string& service_name) = 0;

  // Connect to an interface provided by the remote service provider.
  template <typename Interface>
  void ConnectToRemoteService(mojo::InterfacePtr<Interface>* ptr) {
    mojo::MessagePipe pipe;
    ptr->Bind(pipe.handle0.Pass());
    ConnectToRemoteService(Interface::Name_, pipe.handle1.Pass());
  }
  virtual void ConnectToRemoteService(const base::StringPiece& name,
                                      mojo::ScopedMessagePipeHandle handle) = 0;

 private:
  template <typename Interface>
  static void ForwardToServiceFactory(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>
          service_factory,
      mojo::ScopedMessagePipeHandle handle) {
    service_factory.Run(mojo::MakeRequest<Interface>(handle.Pass()));
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SERVICE_REGISTRY_H_
