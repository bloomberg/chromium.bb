// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
#define MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_

#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/desktop_ui/find_bar_delegate.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/weak_binding_set.h"
#include "ui/views/layout/layout_manager.h"
#include "url/gurl.h"

namespace mojo {
class ApplicationConnection;
class Shell;
class View;
}

namespace mandoline {

class BrowserManager;
class FindBarView;
class ProgressView;
class ToolbarView;

class BrowserWindow : public mus::ViewTreeDelegate,
                      public mojo::ViewTreeHostClient,
                      public web_view::mojom::WebViewClient,
                      public ViewEmbedder,
                      public mojo::InterfaceFactory<ViewEmbedder>,
                      public views::LayoutManager,
                      public FindBarDelegate {
 public:
  BrowserWindow(mojo::ApplicationImpl* app,
                mojo::ViewTreeHostFactory* host_factory,
                BrowserManager* manager);

  void LoadURL(const GURL& url);
  void Close();

  void ShowOmnibox();
  void ShowFind();
  void GoBack();
  void GoForward();

 private:
  ~BrowserWindow() override;

  float DIPSToPixels(float value) const;

  // Overridden from mus::ViewTreeDelegate:
  void OnEmbed(mus::View* root) override;
  void OnConnectionLost(mus::ViewTreeConnection* connection) override;

  // Overridden from ViewTreeHostClient:
  void OnAccelerator(uint32_t id, mojo::EventPtr event) override;

  // Overridden from web_view::mojom::WebViewClient:
  void TopLevelNavigateRequest(mojo::URLRequestPtr request) override;
  void TopLevelNavigationStarted(const mojo::String& url) override;
  void LoadingStateChanged(bool is_loading, double progress) override;
  void BackForwardChanged(web_view::mojom::ButtonState back_button,
                          web_view::mojom::ButtonState forward_button) override;
  void TitleChanged(const mojo::String& title) override;
  void FindInPageMatchCountUpdated(int32_t request_id,
                                   int32_t count,
                                   bool final_update) override;
  void FindInPageSelectionUpdated(int32_t request_id,
                                  int32_t active_match_ordinal) override;

  // Overridden from ViewEmbedder:
  void Embed(mojo::URLRequestPtr request) override;

  // Overridden from mojo::InterfaceFactory<ViewEmbedder>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ViewEmbedder> request) override;


  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  // Overridden from FindBarDelegate:
  void OnDoFind(const std::string& find, bool forward) override;
  void OnHideFindBar() override;

  void Init(mus::View* root);
  void EmbedOmnibox();

  mojo::ApplicationImpl* app_;
  scoped_ptr<AuraInit> aura_init_;
  mojo::ViewTreeHostPtr host_;
  mojo::Binding<ViewTreeHostClient> host_client_binding_;
  BrowserManager* manager_;
  ToolbarView* toolbar_view_;
  ProgressView* progress_bar_;
  FindBarView* find_bar_view_;
  mus::View* root_;
  mus::View* content_;
  mus::View* omnibox_view_;

  mojo::WeakBindingSet<ViewEmbedder> view_embedder_bindings_;

  GURL default_url_;
  GURL current_url_;

  // The active find match.
  int32_t find_active_;

  // The total number of find matches.
  int32_t find_count_;

  web_view::WebView web_view_;

  OmniboxPtr omnibox_;
  scoped_ptr<mojo::ApplicationConnection> omnibox_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
