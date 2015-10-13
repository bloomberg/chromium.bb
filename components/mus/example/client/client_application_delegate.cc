// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/client/client_application_delegate.h"

#include "components/mus/example/wm/wm.mojom.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/aura/native_widget_view_manager.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

ClientApplicationDelegate::ClientApplicationDelegate() : app_(nullptr) {}

ClientApplicationDelegate::~ClientApplicationDelegate() {}

void ClientApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;

  mojom::WMPtr wm;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:example_wm";
  app->ConnectToService(request.Pass(), &wm);

  for (int i = 0; i < 3; ++i) {
    mojo::ViewTreeClientPtr view_tree_client;
    mus::ViewTreeConnection::Create(this, GetProxy(&view_tree_client).Pass());
    wm->Embed(view_tree_client.Pass());
  }
}

bool ClientApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}

void ClientApplicationDelegate::OnEmbed(mus::View* root) {
  if (!aura_init_) {
    aura_init_.reset(
        new mandoline::AuraInit(root, app_->shell(), "example_resources.pak"));
  }

  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
  widget_delegate->GetContentsView()->set_background(
      views::Background::CreateSolidBackground(0xFFDDDDDD));

  views::Widget* widget = new views::Widget;
  // TODO(sky): make this TYPE_WINDOW. I need to fix resources in order to use
  // TYPE_WINDOW.
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = widget_delegate;
  params.native_widget =
      new mandoline::NativeWidgetViewManager(widget, app_->shell(), root);
  params.bounds = root->bounds().To<gfx::Rect>();
  widget->Init(params);
  widget->Show();
}

void ClientApplicationDelegate::OnConnectionLost(
    mus::ViewTreeConnection* connection) {}
