// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/main/main_application_delegate.h"

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"

MainApplicationDelegate::MainApplicationDelegate() {}

MainApplicationDelegate::~MainApplicationDelegate() {}

void MainApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  connections_.push_back(app->ConnectToApplication("mojo:mock_sysui"));
  connections_.push_back(app->ConnectToApplication("mojo:views_examples"));
  connections_.push_back(
      app->ConnectToApplication("exe:window_type_launcher_exe"));
  connections_.push_back(app->ConnectToApplication("exe:chrome"));
}

bool MainApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
