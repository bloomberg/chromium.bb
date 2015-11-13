// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_
#define COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_

#include <map>

#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mojo {
class ApplicationImpl;
}

namespace resource_provider {

class ResourceProviderApp : public mojo::ApplicationDelegate,
                            public mojo::InterfaceFactory<ResourceProvider> {
 public:
  explicit ResourceProviderApp(const std::string& resource_provider_app_url);
  ~ResourceProviderApp() override;

 private:
  // ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<ResourceProvider>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ResourceProvider> request) override;

  mojo::WeakBindingSet<ResourceProvider> bindings_;

  // The name of the app that the resource provider code lives in. When using
  // core services, it'll be the url of that. Otherwise it'll just be
  // mojo:resource_provider.
  std::string resource_provider_app_url_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProviderApp);
};

}  // namespace resource_provider

#endif  // COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_
