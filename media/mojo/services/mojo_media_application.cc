// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application.h"

#include "base/logging.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_renderer_service.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace media {

const char kMojoMediaAppUrl[] = "mojo:media";

// static
GURL MojoMediaApplication::AppUrl() {
  return GURL(kMojoMediaAppUrl);
}

// static
scoped_ptr<mojo::ApplicationDelegate> MojoMediaApplication::CreateApp() {
  return scoped_ptr<mojo::ApplicationDelegate>(new MojoMediaApplication());
}

MojoMediaApplication::MojoMediaApplication() {
}

MojoMediaApplication::~MojoMediaApplication() {
}

void MojoMediaApplication::Initialize(mojo::ApplicationImpl* app) {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);
  // Display process ID, thread ID and timestamp in logs.
  logging::SetLogItems(true, true, true, false);
}

bool MojoMediaApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojo::ContentDecryptionModule>(this);
  connection->AddService<mojo::MediaRenderer>(this);
  return true;
}

void MojoMediaApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::ContentDecryptionModule> request) {
  // The created object is owned by the pipe.
  new MojoCdmService(&cdm_service_context_, request.Pass());
}

void MojoMediaApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::MediaRenderer> request) {
  // The created object is owned by the pipe.
  new MojoRendererService(request.Pass());
}

}  // namespace media
