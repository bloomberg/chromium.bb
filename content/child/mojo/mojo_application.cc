// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/mojo/mojo_application.h"

#include <utility>

#include "base/logging.h"
#include "content/common/application_setup.mojom.h"
#include "mojo/edk/embedder/embedder.h"

namespace content {

MojoApplication::MojoApplication() {
}

MojoApplication::~MojoApplication() {
}

void MojoApplication::InitWithToken(const std::string& token) {
  DCHECK(!interface_registry_.get());
  mojo::ScopedMessagePipeHandle handle =
      mojo::edk::CreateChildMessagePipe(token);
  DCHECK(handle.is_valid());

  mojom::ApplicationSetupPtr application_setup;
  application_setup.Bind(
      mojo::InterfacePtrInfo<mojom::ApplicationSetup>(std::move(handle), 0u));

  interface_registry_.reset(new shell::InterfaceRegistry(nullptr));
  shell::mojom::InterfaceProviderPtr exposed_interfaces;
  interface_registry_->Bind(GetProxy(&exposed_interfaces));

  shell::mojom::InterfaceProviderPtr remote_interfaces;
  shell::mojom::InterfaceProviderRequest remote_interfaces_request =
      GetProxy(&remote_interfaces);
  remote_interfaces_.reset(
      new shell::InterfaceProvider(std::move(remote_interfaces)));
  application_setup->ExchangeInterfaceProviders(
      std::move(remote_interfaces_request),
      std::move(exposed_interfaces));
}

}  // namespace content
