// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/updater/updater_app.h"

#include <utility>

#include "base/logging.h"
#include "mandoline/services/updater/updater_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"

namespace updater {

UpdaterApp::UpdaterApp() : app_impl_(nullptr) {
}

UpdaterApp::~UpdaterApp() {
}

void UpdaterApp::Initialize(mojo::ApplicationImpl* app) {
  app_impl_ = app;
}

bool UpdaterApp::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Updater>(this);
  return true;
}

void UpdaterApp::Create(mojo::ApplicationConnection* connection,
                        mojo::InterfaceRequest<Updater> request) {
  new UpdaterImpl(app_impl_, this, std::move(request));
}

}  // namespace updater

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new updater::UpdaterApp);
  return runner.Run(shell_handle);
}
