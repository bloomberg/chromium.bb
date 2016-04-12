// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_
#define COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_

#include <map>

#include "base/macros.h"
#include "components/resource_provider/public/interfaces/resource_provider.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"

namespace resource_provider {

class ResourceProviderApp : public mojo::ShellClient,
                            public mojo::InterfaceFactory<ResourceProvider> {
 public:
  explicit ResourceProviderApp(const std::string& resource_provider_app_url);
  ~ResourceProviderApp() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<ResourceProvider>:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<ResourceProvider> request) override;

  mojo::TracingImpl tracing_;

  mojo::BindingSet<ResourceProvider> bindings_;

  // The name of the app that the resource provider code lives in. When using
  // core services, it'll be the url of that. Otherwise it'll just be
  // mojo:resource_provider.
  std::string resource_provider_app_url_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProviderApp);
};

}  // namespace resource_provider

#endif  // COMPONENTS_RESOURCE_PROVIDER_RESOURCE_PROVIDER_APP_H_
