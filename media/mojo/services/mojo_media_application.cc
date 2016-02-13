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

#if defined(OS_ANDROID)
#include "media/mojo/services/android_mojo_media_client.h"
#else
#include "media/mojo/services/default_mojo_media_client.h"
#endif

namespace media {

#if defined(OS_ANDROID)
using DefaultClient = AndroidMojoMediaClient;
#else
using DefaultClient = DefaultMojoMediaClient;
#endif

// static
scoped_ptr<mojo::ShellClient> MojoMediaApplication::CreateApp() {
  return scoped_ptr<mojo::ShellClient>(
      new MojoMediaApplication(make_scoped_ptr(new DefaultClient())));
}

// static
scoped_ptr<mojo::ShellClient> MojoMediaApplication::CreateAppWithClient(
    const CreateMojoMediaClientCB& create_mojo_media_client_cb) {
  scoped_ptr<MojoMediaClient> mojo_media_client =
      create_mojo_media_client_cb.Run();
  if (!mojo_media_client)
    return nullptr;

  return scoped_ptr<mojo::ShellClient>(
      new MojoMediaApplication(std::move(mojo_media_client)));
}

// TODO(xhwang): Hook up MediaLog when possible.
MojoMediaApplication::MojoMediaApplication(
    scoped_ptr<MojoMediaClient> mojo_media_client)
    : mojo_media_client_(std::move(mojo_media_client)),
      shell_(nullptr),
      media_log_(new MediaLog()) {
  DCHECK(mojo_media_client_);
}

MojoMediaApplication::~MojoMediaApplication() {}

void MojoMediaApplication::Initialize(mojo::Shell* shell,
                                      const std::string& /* url */,
                                      uint32_t /* id */) {
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
