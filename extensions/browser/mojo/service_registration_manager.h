// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOJO_SERVICE_REGISTRATION_MANAGER_H_
#define EXTENSIONS_BROWSER_MOJO_SERVICE_REGISTRATION_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {
namespace internal {

// A base class for forwarding calls to ServiceRegistry::AddService().
class ServiceFactoryBase {
 public:
  virtual ~ServiceFactoryBase() {}

  // Add this service factory to |service_registry|.
  virtual void Register(content::ServiceRegistry* service_registry) const = 0;
};

template <typename Interface>
class ServiceFactory : public ServiceFactoryBase {
 public:
  explicit ServiceFactory(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& factory)
      : factory_(factory) {}
  ~ServiceFactory() override {}

  void Register(content::ServiceRegistry* service_registry) const override {
    service_registry->AddService(factory_);
  }

 private:
  const base::Callback<void(mojo::InterfaceRequest<Interface>)> factory_;
  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

}  // namespace internal

// A meta service registry. This allows registration of service factories and
// their associated extensions API permission name. Whenever a RenderFrameHost
// is created, each service that the render frame should have access to (based
// on its SiteInstance), is added to the ServiceRegistry for that
// RenderFrameHost.
class ServiceRegistrationManager {
 public:
  ServiceRegistrationManager();
  ~ServiceRegistrationManager();

  static ServiceRegistrationManager* GetSharedInstance();

  // Registers a ServiceFactory to be provided to extensions with the
  // |permission_name| API permission.
  //
  // TODO(sammc): Add support for service factories that take the Extension*
  // (or extension ID) and BrowserContext to allow fine-grained service and
  // permission customization.
  //
  // TODO(sammc): Support more flexible service filters than just API permission
  // names.
  template <typename Interface>
  void AddServiceFactory(
      const std::string& permission_name,
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& factory) {
    factories_.push_back(
        std::make_pair(permission_name,
                       linked_ptr<internal::ServiceFactoryBase>(
                           new internal::ServiceFactory<Interface>(factory))));
  }

  // Adds the service factories appropriate for |render_frame_host| to its
  // ServiceRegistry.
  void AddServicesToRenderFrame(content::RenderFrameHost* render_frame_host);

 private:
  // All factories and their corresponding API permissions.
  std::vector<std::pair<std::string, linked_ptr<internal::ServiceFactoryBase>>>
      factories_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistrationManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOJO_SERVICE_REGISTRATION_MANAGER_H_
