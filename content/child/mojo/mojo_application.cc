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
  mojo::ScopedMessagePipeHandle handle =
      mojo::edk::CreateChildMessagePipe(token);
  DCHECK(handle.is_valid());

  mojom::ApplicationSetupPtr application_setup;
  application_setup.Bind(
      mojo::InterfacePtrInfo<mojom::ApplicationSetup>(std::move(handle), 0u));

  shell::mojom::InterfaceProviderPtr services;
  shell::mojom::InterfaceProviderPtr exposed_services;
  service_registry_.Bind(GetProxy(&exposed_services));
  application_setup->ExchangeInterfaceProviders(GetProxy(&services),
                                                std::move(exposed_services));
  service_registry_.BindRemoteServiceProvider(std::move(services));
}

}  // namespace content
