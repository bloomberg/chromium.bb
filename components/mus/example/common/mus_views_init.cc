// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/common/mus_views_init.h"

#include "components/mus/example/wm/wm.mojom.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_view_manager.h"

MUSViewsInit::MUSViewsInit(mojo::ApplicationImpl* app) : app_(app) {}

MUSViewsInit::~MUSViewsInit() {}

mus::View* MUSViewsInit::CreateWindow() {
  mojom::WMPtr wm;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:example_wm";
  app_->ConnectToService(request.Pass(), &wm);
  mojo::ViewTreeClientPtr view_tree_client;
  mojo::InterfaceRequest<mojo::ViewTreeClient> view_tree_client_request =
      GetProxy(&view_tree_client);
  wm->OpenWindow(view_tree_client.Pass());
  mus::ViewTreeConnection* view_tree_connection =
      mus::ViewTreeConnection::Create(
          this, view_tree_client_request.Pass(),
          mus::ViewTreeConnection::CreateType::WAIT_FOR_EMBED);
  DCHECK(view_tree_connection->GetRoot());
  return view_tree_connection->GetRoot();
}

views::NativeWidget* MUSViewsInit::CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate) {
  return new views::NativeWidgetViewManager(delegate, app_->shell(),
                                            CreateWindow());
}

void MUSViewsInit::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {}

void MUSViewsInit::OnEmbed(mus::View* root) {
  if (!aura_init_) {
    aura_init_.reset(
        new views::AuraInit(root, app_->shell(), "example_resources.pak"));
  }
}

void MUSViewsInit::OnConnectionLost(mus::ViewTreeConnection* connection) {}

#if defined(OS_WIN)
HICON MUSViewsInit::GetSmallWindowIcon() const {
  return nullptr;
}
#endif
