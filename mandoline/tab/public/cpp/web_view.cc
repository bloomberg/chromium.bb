// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/public/cpp/web_view.h"

#include "components/view_manager/public/cpp/view.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace web_view {

WebView::WebView(mojom::WebViewClient* client) : binding_(client) {}
WebView::~WebView() {}

void WebView::Init(mojo::ApplicationImpl* app, mojo::View* view) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:web_view";

  mojom::WebViewClientPtr client;
  mojo::InterfaceRequest<mojom::WebViewClient> client_request =
      GetProxy(&client);
  binding_.Bind(client_request.Pass());

  mojom::WebViewFactoryPtr factory;
  app->ConnectToService(request.Pass(), &factory);
  factory->CreateWebView(client.Pass(), GetProxy(&web_view_));

  mojo::ViewManagerClientPtr view_manager_client;
  web_view_->GetViewManagerClient(GetProxy(&view_manager_client));
  view->Embed(view_manager_client.Pass());
}

}  // namespace web_view
