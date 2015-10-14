// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/client/client_application_delegate.h"

#include "components/mus/example/common/mus_views_init.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

ClientApplicationDelegate::ClientApplicationDelegate() : app_(nullptr) {}

ClientApplicationDelegate::~ClientApplicationDelegate() {}

void ClientApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  mus_views_init_.reset(new MUSViewsInit(app));

  for (int i = 0; i < 3; ++i) {
    views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
    widget_delegate->GetContentsView()->set_background(
        views::Background::CreateSolidBackground(0xFFDDDDDD));

    views::Widget* widget = views::Widget::CreateWindow(widget_delegate);
    widget->Show();
  }
}

bool ClientApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
