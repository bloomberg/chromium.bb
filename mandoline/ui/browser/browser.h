// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_H_
#define MANDOLINE_UI_BROWSER_BROWSER_H_

#include "base/gtest_prod_util.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/tab/frame_tree_delegate.h"
#include "mandoline/ui/browser/navigator_host_impl.h"
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

FORWARD_DECLARE_TEST(BrowserTest, ClosingBrowserClosesAppConnection);

class BrowserDelegate;
class BrowserUI;
class FrameTree;

class Browser : public mojo::ViewManagerDelegate,
                public mojo::ViewManagerRootClient,
                public OmniboxClient,
                public ViewEmbedder,
                public FrameTreeDelegate,
                public mojo::InterfaceFactory<mojo::NavigatorHost>,
                public mojo::InterfaceFactory<ViewEmbedder> {
 public:
  Browser(mojo::ApplicationImpl* app,
          BrowserDelegate* delegate,
          const GURL& default_url);
  ~Browser() override;

  void ReplaceContentWithRequest(mojo::URLRequestPtr request);

  mojo::View* content() { return content_; }
  mojo::View* omnibox() { return omnibox_; }

  const GURL& current_url() const { return current_url_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, ClosingBrowserClosesAppConnection);
  FRIEND_TEST_ALL_PREFIXES(BrowserTest, TwoBrowsers);

  friend class TestBrowser;

  mojo::ApplicationConnection* GetViewManagerConnectionForTesting();

  // Loads |request| in |frame|.
  void NavigateExistingFrame(Frame* frame, mojo::URLRequestPtr request);

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

  // Overridden from FrameTreeDelegate:
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  MessageEvent* event) override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;
  void RequestNavigate(Frame* source,
                       NavigationTargetType target_type,
                       Frame* target_frame,
                       mojo::URLRequestPtr request) override;

  // Overridden from mojo::InterfaceFactory<mojo::NavigatorHost>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NavigatorHost> request) override;

  // Overridden from mojo::InterfaceFactory<ViewEmbedder>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ViewEmbedder> request) override;

  void ShowOmnibox(mojo::URLRequestPtr request);

  mojo::ViewManagerInit view_manager_init_;

  // Only support being embedded once, so both application-level
  // and embedding-level state are shared on the same object.
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_;
  GURL default_url_;
  mojo::URLRequestPtr pending_request_;

  mojo::WeakBindingSet<ViewEmbedder> view_embedder_bindings_;

  NavigatorHostImpl navigator_host_;

  GURL current_url_;

  scoped_ptr<BrowserUI> ui_;
  mojo::ApplicationImpl* app_;
  BrowserDelegate* delegate_;

  scoped_ptr<FrameTree> frame_tree_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_H_
