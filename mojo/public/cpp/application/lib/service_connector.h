// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_

#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace mojo {
class ApplicationConnection;

namespace internal {

class ServiceConnectorBase {
 public:
  ServiceConnectorBase(const std::string& name);
  virtual ~ServiceConnectorBase();
  virtual void ConnectToService(const std::string& name,
                                ScopedMessagePipeHandle client_handle) = 0;
  std::string name() const { return name_; }
  void set_application_connection(ApplicationConnection* connection) {
      application_connection_ = connection; }

 protected:
  std::string name_;
  ApplicationConnection* application_connection_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceConnectorBase);
};

template <typename Interface>
class InterfaceFactoryConnector : public ServiceConnectorBase {
 public:
  explicit InterfaceFactoryConnector(InterfaceFactory<Interface>* factory)
      : ServiceConnectorBase(Interface::Name_), factory_(factory) {}
  virtual ~InterfaceFactoryConnector() {}

  virtual void ConnectToService(const std::string& name,
                                ScopedMessagePipeHandle client_handle) {
    factory_->Create(application_connection_,
                     MakeRequest<Interface>(client_handle.Pass()));
  }

 private:
  InterfaceFactory<Interface>* factory_;
  MOJO_DISALLOW_COPY_AND_ASSIGN(InterfaceFactoryConnector);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_CONNECTOR_H_
