// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/mojo_application_host.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

namespace content {
namespace {

// TODO(beng): remove this in favor of just using shell APIs directly.
//             http://crbug.com/2072603002
class ApplicationSetupImpl : public mojom::ApplicationSetup {
 public:
  ApplicationSetupImpl(ServiceRegistryImpl* service_registry,
                       mojo::InterfaceRequest<mojom::ApplicationSetup> request)
      : binding_(this, std::move(request)),
        service_registry_(service_registry) {}

  ~ApplicationSetupImpl() override {
  }

 private:
  // mojom::ApplicationSetup implementation.
  void ExchangeInterfaceProviders(
      shell::mojom::InterfaceProviderRequest services,
      shell::mojom::InterfaceProviderPtr exposed_services) override {
    service_registry_->Bind(std::move(services));
    mojo::FuseInterface(service_registry_->TakeRemoteRequest(),
                        exposed_services.PassInterface());
  }

  mojo::Binding<mojom::ApplicationSetup> binding_;
  ServiceRegistryImpl* service_registry_;
};

}  // namespace

MojoApplicationHost::MojoApplicationHost(const std::string& child_token)
    : token_(mojo::edk::GenerateRandomToken()) {
#if defined(OS_ANDROID)
  service_registry_android_ =
      ServiceRegistryAndroid::Create(&service_registry_);
#endif

  mojo::ScopedMessagePipeHandle pipe =
      mojo::edk::CreateParentMessagePipe(token_, child_token);
  DCHECK(pipe.is_valid());
  application_setup_.reset(new ApplicationSetupImpl(
      &service_registry_,
      mojo::MakeRequest<mojom::ApplicationSetup>(std::move(pipe))));
}

MojoApplicationHost::~MojoApplicationHost() {
}

}  // namespace content
