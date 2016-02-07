// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/web_view_application_delegate.h"

#include <utility>

#include "components/web_view/web_view_impl.h"
#include "mojo/shell/public/cpp/connection.h"

namespace web_view {

WebViewApplicationDelegate::WebViewApplicationDelegate() : shell_(nullptr) {}
WebViewApplicationDelegate::~WebViewApplicationDelegate() {}

void WebViewApplicationDelegate::Initialize(mojo::Shell* shell,
                                            const std::string& url,
                                            uint32_t id) {
  shell_ = shell;
  tracing_.Initialize(shell, url);
}

bool WebViewApplicationDelegate::AcceptConnection(
    mojo::Connection* connection) {
  connection->AddService<mojom::WebViewFactory>(this);
  return true;
}

void WebViewApplicationDelegate::CreateWebView(
    mojom::WebViewClientPtr client,
    mojo::InterfaceRequest<mojom::WebView> web_view) {
  new WebViewImpl(shell_, std::move(client), std::move(web_view));
}

void WebViewApplicationDelegate::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<mojom::WebViewFactory> request) {
  factory_bindings_.AddBinding(this, std::move(request));
}

}  // namespace web_view
