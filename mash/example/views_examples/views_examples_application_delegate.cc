// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/example/views_examples/views_examples_application_delegate.h"

#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"

ViewsExamplesApplicationDelegate::ViewsExamplesApplicationDelegate() {}

ViewsExamplesApplicationDelegate::~ViewsExamplesApplicationDelegate() {
}

void ViewsExamplesApplicationDelegate::Initialize(mojo::Shell* shell,
                                                  const std::string& url,
                                                  uint32_t id) {
  tracing_.Initialize(shell, url);
  aura_init_.reset(new views::AuraInit(shell, "views_mus_resources.pak"));

  views::WindowManagerConnection::Create(shell);

  views::examples::ShowExamplesWindow(views::examples::DO_NOTHING_ON_CLOSE,
                                      nullptr, nullptr);
}

bool ViewsExamplesApplicationDelegate::AcceptConnection(
    mojo::Connection* connection) {
  return false;
}
