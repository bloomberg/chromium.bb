// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"

#include <utility>

#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/service_factory_impl.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"

namespace media {

// static
scoped_ptr<mojo::ShellClient> MojoMediaApplication::CreateApp() {
  return scoped_ptr<mojo::ShellClient>(
      new MojoMediaApplication(MojoMediaClient::Create()));
}

// TODO(xhwang): Hook up MediaLog when possible.
MojoMediaApplication::MojoMediaApplication(
    scoped_ptr<MojoMediaClient> mojo_media_client)
    : mojo_media_client_(std::move(mojo_media_client)),
      shell_(nullptr),
      media_log_(new MediaLog()) {}

MojoMediaApplication::~MojoMediaApplication() {}

void MojoMediaApplication::Initialize(mojo::Shell* shell,
                                      const std::string& url,
                                      uint32_t id) {
  shell_ = shell;
  mojo_media_client_->Initialize();
}

bool MojoMediaApplication::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<interfaces::ServiceFactory>(this);
  return true;
}

void MojoMediaApplication::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<interfaces::ServiceFactory> request) {
  // The created object is owned by the pipe.
  new ServiceFactoryImpl(std::move(request), connection->GetRemoteInterfaces(),
                         media_log_, shell_->CreateAppRefCount(),
                         mojo_media_client_.get());
}

}  // namespace media
