// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_WEB_VIEW_APPLICATION_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_WEB_VIEW_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace web_view {

class WebViewApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojom::WebViewFactory,
      public mojo::InterfaceFactory<mojom::WebViewFactory> {
 public:
  WebViewApplicationDelegate();
  ~WebViewApplicationDelegate() override;

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojom::WebViewFactory:
  void CreateWebView(mojom::WebViewClientPtr client,
                     mojo::InterfaceRequest<mojom::WebView> web_view) override;

  // Overridden from mojo::InterfaceFactory<mojom::WebView>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::WebViewFactory> request) override;

  mojo::Shell* shell_;
  mojo::TracingImpl tracing_;

  mojo::WeakBindingSet<WebViewFactory> factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WebViewApplicationDelegate);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_WEB_VIEW_APPLICATION_DELEGATE_H_
