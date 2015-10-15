// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/views_examples/views_examples_application_delegate.h"

#include "components/mus/example/common/mus_views_init.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"

ViewsExamplesApplicationDelegate::ViewsExamplesApplicationDelegate()
    : app_(nullptr) {}

ViewsExamplesApplicationDelegate::~ViewsExamplesApplicationDelegate() {
}

void ViewsExamplesApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  mus_views_init_.reset(new MUSViewsInit(app));

  views::examples::ShowExamplesWindow(views::examples::DO_NOTHING_ON_CLOSE,
                                      nullptr, nullptr);
}

bool ViewsExamplesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
