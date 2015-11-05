// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mojo_runner_state.h"

#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/runner/child/runner_connection.h"

class ChromeApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  ChromeApplicationDelegate() {}
  ~ChromeApplicationDelegate() override {}

 private:
  void Initialize(mojo::ApplicationImpl* application) override {
    // TODO(beng): Connect to the window manager.
  }
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeApplicationDelegate);
};

MojoRunnerState::MojoRunnerState() {}
MojoRunnerState::~MojoRunnerState() {}

void MojoRunnerState::WaitForConnection() {
  mojo::InterfaceRequest<mojo::Application> application_request;
  runner_connection_.reset(
      mojo::runner::RunnerConnection::ConnectToRunner(&application_request));
  application_delegate_.reset(new ChromeApplicationDelegate);
  application_impl_.reset(new mojo::ApplicationImpl(
      application_delegate_.get(), application_request.Pass()));
  application_impl_->WaitForInitialize();
}
