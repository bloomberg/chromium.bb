// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/public/cpp/web_view.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "components/mus/public/cpp/window.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace web_view {
namespace {

void OnEmbed(bool success, uint16_t connection_id) {
  CHECK(success);
}

}  // namespace

WebView::WebView(mojom::WebViewClient* client) : binding_(client) {}
WebView::~WebView() {}

void WebView::Init(mojo::ApplicationImpl* app, mus::Window* window) {
  mojom::WebViewFactoryPtr factory;
  app->ConnectToService("mojo:web_view", &factory);
  factory->CreateWebView(binding_.CreateInterfacePtrAndBind(),
                         GetProxy(&web_view_));

  mus::mojom::WindowTreeClientPtr window_tree_client;
  web_view_->GetWindowTreeClient(GetProxy(&window_tree_client));
  window->Embed(std::move(window_tree_client),
                mus::mojom::WindowTree::kAccessPolicyEmbedRoot,
                base::Bind(&OnEmbed));
}

}  // namespace web_view
