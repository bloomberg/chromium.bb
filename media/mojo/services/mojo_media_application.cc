// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"

#include <utility>

#include "base/logging.h"
#include "media/base/media_log.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/service_factory_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace media {

// static
scoped_ptr<mojo::ApplicationDelegate> MojoMediaApplication::CreateApp() {
  // In all existing use cases we don't need to initialize logging when using
  // CreateApp() to create the application. We can pass |enable_logging| in
  // CreateApp() if this isn't the case any more in the future.
  return scoped_ptr<mojo::ApplicationDelegate>(
      new MojoMediaApplication(false, MojoMediaClient::Create()));
}

// TODO(xhwang): Hook up MediaLog when possible.
MojoMediaApplication::MojoMediaApplication(
    bool enable_logging,
    scoped_ptr<MojoMediaClient> mojo_media_client)
    : enable_logging_(enable_logging),
      mojo_media_client_(std::move(mojo_media_client)),
      app_impl_(nullptr),
      media_log_(new MediaLog()) {}

MojoMediaApplication::~MojoMediaApplication() {
}

void MojoMediaApplication::Initialize(mojo::ApplicationImpl* app) {
  app_impl_ = app;

  if (enable_logging_) {
    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::InitLogging(settings);
    // Display process ID, thread ID and timestamp in logs.
    logging::SetLogItems(true, true, true, false);
  }
  mojo_media_client_->Initialize();
}

bool MojoMediaApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<interfaces::ServiceFactory>(this);
  return true;
}

void MojoMediaApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<interfaces::ServiceFactory> request) {
  // The created object is owned by the pipe.
  new ServiceFactoryImpl(std::move(request), connection->GetServiceProvider(),
                         media_log_,
                         app_impl_->app_lifetime_helper()->CreateAppRefCount(),
                         mojo_media_client_.get());
}

}  // namespace media
