// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
#define MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_

#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mandoline/tab/public/cpp/web_view.h"
#include "mandoline/tab/public/interfaces/web_view.mojom.h"
#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/layout_manager.h"
#include "url/gurl.h"

namespace mojo {
class ApplicationConnection;
class Shell;
class View;
}

namespace views {
class LabelButton;
}

namespace mandoline {

class BrowserManager;
class ProgressView;

class BrowserWindow : public mojo::ViewManagerDelegate,
                      public mojo::ViewManagerRootClient,
                      public web_view::mojom::WebViewClient,
                      public ViewEmbedder,
                      public mojo::InterfaceFactory<ViewEmbedder>,
                      public views::LayoutManager,
                      public views::ButtonListener {
 public:
  BrowserWindow(mojo::ApplicationImpl* app, BrowserManager* manager);
  ~BrowserWindow() override;

  void LoadURL(const GURL& url);

 private:
  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnViewManagerDestroyed(mojo::ViewManager* view_manager) override;

  // Overridden from ViewManagerRootClient:
  void OnAccelerator(mojo::EventPtr event) override;

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigate(mojo::URLRequestPtr request) override;
  void LoadingStateChanged(bool is_loading) override;
  void ProgressChanged(double progress) override;

  // Overridden from ViewEmbedder:
  void Embed(mojo::URLRequestPtr request) override;

  // Overridden from mojo::InterfaceFactory<ViewEmbedder>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ViewEmbedder> request) override;


  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void Init(mojo::View* root);
  void ShowOmnibox();
  void EmbedOmnibox(mojo::ApplicationConnection* connection);

  mojo::ApplicationImpl* app_;
  scoped_ptr<AuraInit> aura_init_;
  mojo::ViewManagerInit view_manager_init_;
  BrowserManager* manager_;
  views::LabelButton* omnibox_launcher_;
  ProgressView* progress_bar_;
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_view_;

  mojo::WeakBindingSet<ViewEmbedder> view_embedder_bindings_;

  GURL default_url_;
  GURL current_url_;

  web_view::WebView web_view_;

  OmniboxPtr omnibox_;
  scoped_ptr<mojo::ApplicationConnection> omnibox_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
