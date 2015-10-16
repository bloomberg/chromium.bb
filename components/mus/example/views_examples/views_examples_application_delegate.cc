// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/views_examples/views_examples_application_delegate.h"

#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/network/network_type_converters.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/window_manager_connection.h"

ViewsExamplesApplicationDelegate::ViewsExamplesApplicationDelegate() {}

ViewsExamplesApplicationDelegate::~ViewsExamplesApplicationDelegate() {
}

void ViewsExamplesApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  mus::mojom::WindowManagerPtr window_manager;
  app->ConnectToService(mojo::URLRequest::From(std::string("mojo:example_wm")),
                        &window_manager);
  views::WindowManagerConnection::Create(window_manager.Pass(), app);

  views::examples::ShowExamplesWindow(views::examples::DO_NOTHING_ON_CLOSE,
                                      nullptr, nullptr);
}

bool ViewsExamplesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
