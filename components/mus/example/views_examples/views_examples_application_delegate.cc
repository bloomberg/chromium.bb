// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/views_examples/views_examples_application_delegate.h"

#include "components/mus/example/common/mus_views_init.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/widget/widget_delegate.h"

ViewsExamplesApplicationDelegate::ViewsExamplesApplicationDelegate()
    : app_(nullptr) {}

ViewsExamplesApplicationDelegate::~ViewsExamplesApplicationDelegate() {
}

void ViewsExamplesApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  mus_views_init_.reset(new MUSViewsInit(app));

  // TODO(sky): total hack! This is necessary as WindowTypeLauncherView is
  // created before AuraInit. WindowTypeLauncherView uses resources that are
  // configured by MUSViewsInit once a View is created. By creating a Widget
  // here we ensure the necessary state has been setup.
  views::Widget::CreateWindow(new views::WidgetDelegateView);

  views::examples::ShowExamplesWindow(views::examples::DO_NOTHING_ON_CLOSE,
                                      nullptr, nullptr);
}

bool ViewsExamplesApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
