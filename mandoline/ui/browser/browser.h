// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_H_
#define MANDOLINE_UI_BROWSER_BROWSER_H_

#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/ui/browser/navigator_host_impl.h"
#include "mandoline/ui/browser/public/interfaces/launch_handler.mojom.h"
#include "mandoline/ui/browser/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/browser/public/interfaces/view_embedder.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/common/weak_binding_set.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "url/gurl.h"

namespace mojo {
class ViewManagerInit;
}

namespace mandoline {

class BrowserUI;
class FrameTree;

class Browser : public mojo::ApplicationDelegate,
                public mojo::ViewManagerDelegate,
                public mojo::ViewManagerRootClient,
                public OmniboxClient,
                public ViewEmbedder,
                public LaunchHandler,
                public mojo::InterfaceFactory<mojo::NavigatorHost>,
                public mojo::InterfaceFactory<ViewEmbedder>,
                public mojo::InterfaceFactory<LaunchHandler> {
 public:
  Browser();
  ~Browser() override;

  void ReplaceContentWithRequest(mojo::URLRequestPtr request);

  mojo::View* content() { return content_; }
  mojo::View* omnibox() { return omnibox_; }

  const GURL& current_url() const { return current_url_; }

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  bool ConfigureOutgoingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnEmbedForDescendant(mojo::View* view,
                            mojo::URLRequestPtr request,
                            mojo::ViewManagerClientPtr* client) override;
  void OnViewManagerDestroyed(mojo::ViewManager* view_manager) override;

  // Overridden from ViewManagerRootClient:
  void OnAccelerator(mojo::EventPtr event) override;

  // Overridden from OmniboxClient:
  void OpenURL(const mojo::String& url) override;

  // Overridden from ViewEmbedder:
  void Embed(mojo::URLRequestPtr request) override;

  // Overridden from LaunchHandler:
  void LaunchURL(const mojo::String& url) override;

  // Overridden from mojo::InterfaceFactory<mojo::NavigatorHost>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NavigatorHost> request) override;

  // Overridden from mojo::InterfaceFactory<ViewEmbedder>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ViewEmbedder> request) override;

  // Overridden from mojo::InterfaceFactory<LaunchHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<LaunchHandler> request) override;

  void ShowOmnibox(mojo::URLRequestPtr request);

  scoped_ptr<mojo::ViewManagerInit> view_manager_init_;

  // Only support being embedded once, so both application-level
  // and embedding-level state are shared on the same object.
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_;
  std::string default_url_;
  mojo::URLRequestPtr pending_request_;

  mojo::WeakBindingSet<ViewEmbedder> view_embedder_bindings_;
  mojo::WeakBindingSet<LaunchHandler> launch_handler_bindings_;

  NavigatorHostImpl navigator_host_;

  GURL current_url_;

  scoped_ptr<BrowserUI> ui_;
  mojo::ApplicationImpl* app_;

  scoped_ptr<FrameTree> frame_tree_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_H_
