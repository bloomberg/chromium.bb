// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/updater/updater_app.h"

#include <utility>

#include "base/logging.h"
#include "mandoline/services/updater/updater_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/shell.h"

namespace updater {

UpdaterApp::UpdaterApp() : shell_(nullptr) {
}

UpdaterApp::~UpdaterApp() {
}

void UpdaterApp::Initialize(mojo::Shell* shell, const std::string& url,
                            uint32_t id) {
  shell_ = shell;
}

bool UpdaterApp::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Updater>(this);
  return true;
}

void UpdaterApp::Create(mojo::ApplicationConnection* connection,
                        mojo::InterfaceRequest<Updater> request) {
  new UpdaterImpl(this, std::move(request));
}

}  // namespace updater

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new updater::UpdaterApp);
  return runner.Run(shell_handle);
}
