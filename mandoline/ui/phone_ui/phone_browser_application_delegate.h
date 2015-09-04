// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_
#define MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "components/view_manager/public/cpp/view_tree_delegate.h"
#include "components/view_manager/public/interfaces/view_tree_host.mojom.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
// TODO(beng): move this file somewhere common.
#include "mandoline/ui/desktop_ui/public/interfaces/launch_handler.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"

namespace mojo {
class View;
}

namespace mandoline {

class PhoneBrowserApplicationDelegate :
    public mojo::ApplicationDelegate,
    public LaunchHandler,
    public mojo::ViewTreeDelegate,
    public mojo::ViewObserver,
    public web_view::mojom::WebViewClient,
    public mojo::InterfaceFactory<LaunchHandler> {
 public:
  PhoneBrowserApplicationDelegate();
  ~PhoneBrowserApplicationDelegate() override;

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from LaunchHandler:
  void LaunchURL(const mojo::String& url) override;

  // Overridden from mojo::ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnConnectionLost(mojo::ViewTreeConnection* connection) override;

  // Overridden from mojo::ViewObserver:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigate(mojo::URLRequestPtr request) override;
  void LoadingStateChanged(bool is_loading) override;
  void ProgressChanged(double progress) override;
  void TitleChanged(const mojo::String& title) override;

  // Overridden from mojo::InterfaceFactory<LaunchHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<LaunchHandler> request) override;

  mojo::ApplicationImpl* app_;
  mojo::ViewTreeHostPtr host_;

  mojo::View* root_;
  mojo::View* content_;
  web_view::WebView web_view_;

  mojo::String default_url_;

  mojo::WeakBindingSet<LaunchHandler> launch_handler_bindings_;

  DISALLOW_COPY_AND_ASSIGN(PhoneBrowserApplicationDelegate);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_
