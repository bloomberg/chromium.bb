// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_
#define MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
// TODO(beng): move this file somewhere common.
#include "mandoline/ui/desktop_ui/public/interfaces/launch_handler.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mus {
class View;
}

namespace mandoline {

class PhoneBrowserApplicationDelegate
    : public mojo::ApplicationDelegate,
      public LaunchHandler,
      public mus::WindowTreeDelegate,
      public mus::WindowObserver,
      public web_view::mojom::WebViewClient,
      public mojo::InterfaceFactory<LaunchHandler> {
 public:
  PhoneBrowserApplicationDelegate();
  ~PhoneBrowserApplicationDelegate() override;

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from LaunchHandler:
  void LaunchURL(const mojo::String& url) override;

  // Overridden from mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // Overridden from mus::WindowObserver:
  void OnWindowBoundsChanged(mus::Window* view,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigateRequest(mojo::URLRequestPtr request) override;
  void TopLevelNavigationStarted(const mojo::String& url) override;
  void LoadingStateChanged(bool is_loading, double progress) override;
  void BackForwardChanged(web_view::mojom::ButtonState back_button,
                          web_view::mojom::ButtonState forward_button) override;
  void TitleChanged(const mojo::String& title) override;
  void FindInPageMatchCountUpdated(int32_t request_id,
                                   int32_t count,
                                   bool final_update) override {}
  void FindInPageSelectionUpdated(int32_t request_id,
                                  int32_t active_match_ordinal) override {}

  // Overridden from mojo::InterfaceFactory<LaunchHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<LaunchHandler> request) override;

  mojo::ApplicationImpl* app_;
  mus::mojom::WindowTreeHostPtr host_;

  mus::Window* root_;
  mus::Window* content_;
  web_view::WebView web_view_;

  mojo::String default_url_;

  mojo::WeakBindingSet<LaunchHandler> launch_handler_bindings_;

  DISALLOW_COPY_AND_ASSIGN(PhoneBrowserApplicationDelegate);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_PHONE_UI_PHONE_BROWSER_APPLICATION_DELEGATE_H_
