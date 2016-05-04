// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_mojo_media_application.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromecast/browser/media/cast_mojo_media_client.h"
#include "media/base/media_log.h"
#include "media/mojo/services/service_factory_impl.h"
#include "services/shell/public/cpp/connection.h"

namespace {
void CreateServiceFactory(
    mojo::InterfaceRequest<media::interfaces::ServiceFactory> request,
    shell::mojom::InterfaceProvider* interfaces,
    scoped_refptr<media::MediaLog> media_log,
    std::unique_ptr<shell::ShellConnectionRef> connection_ref,
    media::MojoMediaClient* mojo_media_client) {
  new ::media::ServiceFactoryImpl(std::move(request), interfaces,
                                  std::move(media_log),
                                  std::move(connection_ref), mojo_media_client);
}
}  // namespace

namespace chromecast {
namespace media {

CastMojoMediaApplication::CastMojoMediaApplication(
    std::unique_ptr<CastMojoMediaClient> mojo_media_client,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
    const base::Closure& quit_closure)
    : mojo_media_client_(std::move(mojo_media_client)),
      media_task_runner_(media_task_runner),
      media_log_(new ::media::MediaLog()),
      ref_factory_(quit_closure) {
  DCHECK(mojo_media_client_);
  DCHECK(media_task_runner_);
}

CastMojoMediaApplication::~CastMojoMediaApplication() {}

void CastMojoMediaApplication::Initialize(::shell::Connector* connector,
                                          const ::shell::Identity& identity,
                                          uint32_t /* id */) {}

bool CastMojoMediaApplication::AcceptConnection(
    ::shell::Connection* connection) {
  connection->AddInterface<::media::interfaces::ServiceFactory>(this);
  return true;
}

void CastMojoMediaApplication::Create(
    ::shell::Connection* connection,
    mojo::InterfaceRequest<::media::interfaces::ServiceFactory> request) {
  // Create a connection ref here to keep the app alive until the ServiceFactory
  // is done with it.
  std::unique_ptr<shell::ShellConnectionRef> connection_ref =
      ref_factory_.CreateRef();
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CreateServiceFactory, base::Passed(&request),
                 connection->GetRemoteInterfaces(), media_log_,
                 base::Passed(&connection_ref), mojo_media_client_.get()));
}

}  // namespace media
}  // namespace chromecast
