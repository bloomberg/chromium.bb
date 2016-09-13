// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"

#include <utility>

#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/service_factory_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

namespace media {

// TODO(xhwang): Hook up MediaLog when possible.
MojoMediaApplication::MojoMediaApplication(
    std::unique_ptr<MojoMediaClient> mojo_media_client,
    const base::Closure& quit_closure)
    : mojo_media_client_(std::move(mojo_media_client)),
      media_log_(new MediaLog()),
      ref_factory_(quit_closure) {
  DCHECK(mojo_media_client_);
}

MojoMediaApplication::~MojoMediaApplication() {}

void MojoMediaApplication::OnStart(const shell::Identity& identity) {
  mojo_media_client_->Initialize();
}

bool MojoMediaApplication::OnConnect(const shell::Identity& remote_identity,
                                     shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::MediaService>(this);
  return true;
}

bool MojoMediaApplication::OnStop() {
  mojo_media_client_.reset();
  return true;
}

void MojoMediaApplication::Create(const shell::Identity& remote_identity,
                                  mojom::MediaServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MojoMediaApplication::CreateServiceFactory(
    mojom::ServiceFactoryRequest request,
    shell::mojom::InterfaceProviderPtr remote_interfaces) {
  // Ignore request if service has already stopped.
  if (!mojo_media_client_)
    return;

  mojo::MakeStrongBinding(
      base::MakeUnique<ServiceFactoryImpl>(std::move(remote_interfaces),
                                           media_log_, ref_factory_.CreateRef(),
                                           mojo_media_client_.get()),
      std::move(request));
}

}  // namespace media
