// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/public/cpp/web_view.h"

#include "base/bind.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace web_view {
namespace {

void OnEmbed(bool success, uint16 connection_id) {
  CHECK(success);
}

}  // namespace

WebView::WebView(mojom::WebViewClient* client) : binding_(client) {}
WebView::~WebView() {}

void WebView::Init(mojo::ApplicationImpl* app, mus::Window* window) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:web_view";

  mojom::WebViewClientPtr client;
  mojo::InterfaceRequest<mojom::WebViewClient> client_request =
      GetProxy(&client);
  binding_.Bind(client_request.Pass());

  mojom::WebViewFactoryPtr factory;
  app->ConnectToService(request.Pass(), &factory);
  factory->CreateWebView(client.Pass(), GetProxy(&web_view_));

  mus::mojom::WindowTreeClientPtr window_tree_client;
  web_view_->GetWindowTreeClient(GetProxy(&window_tree_client));
  window->Embed(window_tree_client.Pass(),
                mus::mojom::WindowTree::ACCESS_POLICY_EMBED_ROOT,
                base::Bind(&OnEmbed));
}

}  // namespace web_view
