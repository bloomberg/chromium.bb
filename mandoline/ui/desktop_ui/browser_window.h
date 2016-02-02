// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_
#define MANDOLINE_UI_DESKTOP_UI_BROWSER_WINDOW_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "components/web_view/public/cpp/web_view.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "mandoline/ui/desktop_ui/find_bar_delegate.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "url/gurl.h"

namespace mojo {
class ApplicationConnection;
class Shell;
}

namespace ui {
namespace mojo {
class UIInit;
}
}

namespace views {
class AuraInit;
class View;
}

namespace mandoline {

class BrowserManager;
class FindBarView;
class ProgressView;
class ToolbarView;

class BrowserWindow : public mus::WindowTreeDelegate,
                      public web_view::mojom::WebViewClient,
                      public ViewEmbedder,
                      public mojo::InterfaceFactory<ViewEmbedder>,
                      public FindBarDelegate,
                      public mus::WindowManagerDelegate {
 public:
  BrowserWindow(mojo::ApplicationImpl* app,
                mus::mojom::WindowTreeHostFactory* host_factory,
                BrowserManager* manager);

  void LoadURL(const GURL& url);
  void Close();

  void ShowOmnibox();
  void ShowFind();
  void GoBack();
  void GoForward();

 private:
  class LayoutManagerImpl;

  ~BrowserWindow() override;

  float DIPSToPixels(float value) const;

  // Overridden from mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // Overridden from WindowManagerDelegate:
  void SetWindowManagerClient(mus::WindowManagerClient* client) override;
  bool OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(mus::Window* window,
                       const std::string& name,
                       scoped_ptr<std::vector<uint8_t>>* new_data) override;
  mus::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnAccelerator(uint32_t id, mus::mojom::EventPtr event) override;

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


  // Overridden from FindBarDelegate:
  void OnDoFind(const std::string& find, bool forward) override;
  void OnHideFindBar() override;

  void Init(mus::Window* root);
  void EmbedOmnibox();

  void Layout(views::View* host);

  mojo::ApplicationImpl* app_;
  mus::WindowManagerClient* window_manager_client_;
  scoped_ptr<ui::mojo::UIInit> ui_init_;
  scoped_ptr<views::AuraInit> aura_init_;
  mus::mojom::WindowTreeHostPtr host_;
  BrowserManager* manager_;
  ToolbarView* toolbar_view_;
  ProgressView* progress_bar_;
  FindBarView* find_bar_view_;
  mus::Window* root_;
  mus::Window* content_;
  mus::Window* omnibox_view_;

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
