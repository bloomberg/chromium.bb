// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"

namespace mojo {

class ApplicationImpl;

namespace internal {

class ServiceConnectorBase;

// A ServiceRegistry represents each half of a connection between two
// applications, allowing customization of which services are published to the
// other.
class ServiceRegistry : public ServiceProvider, public ApplicationConnection {
 public:
  ServiceRegistry();
  ServiceRegistry(ApplicationImpl* application_impl,
                  const std::string& url,
                  ServiceProviderPtr service_provider);
  virtual ~ServiceRegistry();

  // ApplicationConnection overrides.
  virtual void AddServiceConnector(ServiceConnectorBase* service_connector)
      MOJO_OVERRIDE;
  virtual const std::string& GetRemoteApplicationURL() MOJO_OVERRIDE;
  virtual ApplicationConnection* ConnectToApplication(
      const std::string& url) MOJO_OVERRIDE;
  virtual ServiceProvider* GetServiceProvider() MOJO_OVERRIDE;

  virtual void RemoveServiceConnector(ServiceConnectorBase* service_connector);

 private:
  // ServiceProvider method.
  virtual void ConnectToService(const mojo::String& service_name,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE;

  ApplicationImpl* application_impl_;
  const std::string url_;

 private:
  bool RemoveServiceConnectorInternal(
      ServiceConnectorBase* service_connector);

  Application* application_;
  typedef std::map<std::string, ServiceConnectorBase*>
      NameToServiceConnectorMap;
  NameToServiceConnectorMap name_to_service_connector_;
  ServiceProviderPtr remote_service_provider_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceRegistry);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_APPLICATION_LIB_SERVICE_REGISTRY_H_
