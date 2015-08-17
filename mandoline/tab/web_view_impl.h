// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_WEB_VIEW_IMPL_H_
#define MANDOLINE_TAB_WEB_VIEW_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "mandoline/tab/frame_tree_delegate.h"
#include "mandoline/tab/public/interfaces/web_view.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace mandoline {
class Frame;
class FrameTree;
class HTMLMessageEvent;
}

namespace mojo {
class ApplicationImpl;
}

// TODO(beng): remove once these classes are in the web_view namespace.
using mandoline::Frame;
using mandoline::FrameTree;
using mandoline::HTMLMessageEvent;

namespace web_view {

class WebViewImpl : public mojom::WebView,
                    public mojo::ViewManagerDelegate,
                    public mojo::ViewObserver,
                    public mandoline::FrameTreeDelegate {
 public:
  WebViewImpl(mojo::ApplicationImpl* app,
              mojom::WebViewClientPtr client,
              mojo::InterfaceRequest<WebView> request);
  ~WebViewImpl() override;

 private:
  // Overridden from WebView:
  void LoadRequest(mojo::URLRequestPtr request) override;
  void GetViewManagerClient(
      mojo::InterfaceRequest<mojo::ViewManagerClient> view_manager_client)
          override;

  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnEmbedForDescendant(mojo::View* view,
                            mojo::URLRequestPtr request,
                            mojo::ViewManagerClientPtr* client) override;
  void OnViewManagerDestroyed(mojo::ViewManager* view_manager) override;

  // Overridden from mojo::ViewObserver:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;

  // Overridden from mandoline::FrameTreeDelegate:
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  HTMLMessageEvent* event) override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;
  void RequestNavigate(Frame* source,
                       mandoline::NavigationTargetType target_type,
                       Frame* target_frame,
                       mojo::URLRequestPtr request) override;

  // Loads |request| in |frame|.
  void NavigateExistingFrame(Frame* frame, mojo::URLRequestPtr request);

  mojo::ApplicationImpl* app_;
  mojom::WebViewClientPtr client_;
  mojo::StrongBinding<WebView> binding_;
  mojo::View* content_;
  scoped_ptr<FrameTree> frame_tree_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;

  mojo::URLRequestPtr pending_request_;

  DISALLOW_COPY_AND_ASSIGN(WebViewImpl);
};

}  // namespace web_view

#endif  // MANDOLINE_TAB_WEB_VIEW_IMPL_H_
