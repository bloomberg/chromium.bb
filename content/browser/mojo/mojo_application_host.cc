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
  ApplicationSetupImpl(shell::InterfaceRegistry* interface_registry,
                       shell::InterfaceProvider* remote_interfaces,
                       mojo::InterfaceRequest<mojom::ApplicationSetup> request)
      : binding_(this, std::move(request)),
        interface_registry_(interface_registry),
        remote_interfaces_(remote_interfaces) {
    shell::mojom::InterfaceProviderPtr remote;
    pending_interfaces_request_ = GetProxy(&remote);
    remote_interfaces_->Bind(std::move(remote));
  }

  ~ApplicationSetupImpl() override {
  }

 private:
  // mojom::ApplicationSetup implementation.
  void ExchangeInterfaceProviders(
      shell::mojom::InterfaceProviderRequest request,
      shell::mojom::InterfaceProviderPtr remote_interfaces) override {
    mojo::FuseInterface(std::move(pending_interfaces_request_),
                        remote_interfaces.PassInterface());
    interface_registry_->Bind(std::move(request));
  }

  mojo::Binding<mojom::ApplicationSetup> binding_;
  shell::InterfaceRegistry* interface_registry_;
  shell::InterfaceProvider* remote_interfaces_;
  shell::mojom::InterfaceProviderRequest pending_interfaces_request_;
};

}  // namespace

MojoApplicationHost::MojoApplicationHost(const std::string& child_token)
    : token_(mojo::edk::GenerateRandomToken()),
      interface_registry_(new shell::InterfaceRegistry(nullptr)),
      remote_interfaces_(new shell::InterfaceProvider) {
#if defined(OS_ANDROID)
  service_registry_android_ = ServiceRegistryAndroid::Create(
      interface_registry_.get(), remote_interfaces_.get());
#endif

  mojo::ScopedMessagePipeHandle pipe =
      mojo::edk::CreateParentMessagePipe(token_, child_token);
  DCHECK(pipe.is_valid());
  application_setup_.reset(new ApplicationSetupImpl(
      interface_registry_.get(),
      remote_interfaces_.get(),
      mojo::MakeRequest<mojom::ApplicationSetup>(std::move(pipe))));
}

MojoApplicationHost::~MojoApplicationHost() {
}

}  // namespace content
