// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_
#define COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/view_observer.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/web_view/frame_devtools_agent_delegate.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {
class ApplicationImpl;
}

namespace web_view {

class Frame;
class FrameTree;
class FrameDevToolsAgent;
class HTMLMessageEvent;
class PendingWebViewLoad;

class WebViewImpl : public mojom::WebView,
                    public mojo::ViewTreeDelegate,
                    public mojo::ViewObserver,
                    public FrameTreeDelegate,
                    public FrameDevToolsAgentDelegate {
 public:
  WebViewImpl(mojo::ApplicationImpl* app,
              mojom::WebViewClientPtr client,
              mojo::InterfaceRequest<WebView> request);
  ~WebViewImpl() override;

 private:
  friend class PendingWebViewLoad;

  // See description above |pending_load_| for details.
  void OnLoad();

  // Overridden from WebView:
  void LoadRequest(mojo::URLRequestPtr request) override;
  void GetViewTreeClient(
      mojo::InterfaceRequest<mojo::ViewTreeClient> view_tree_client)
          override;

  // Overridden from mojo::ViewTreeDelegate:
  void OnEmbed(mojo::View* root) override;
  void OnConnectionLost(mojo::ViewTreeConnection* connection) override;

  // Overridden from mojo::ViewObserver:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override;
  void OnViewDestroyed(mojo::View* view) override;

  // Overridden from FrameTreeDelegate:
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  HTMLMessageEvent* event) override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;
  void TitleChanged(const mojo::String& title) override;
  void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) override;
  void CanNavigateFrame(Frame* target,
                        mojo::URLRequestPtr request,
                        const CanNavigateFrameCallback& callback) override;
  void DidStartNavigation(Frame* frame) override;

  // Overridden from FrameDevToolsAgent::Delegate:
  void HandlePageNavigateRequest(const GURL& url) override;

  mojo::ApplicationImpl* app_;
  mojom::WebViewClientPtr client_;
  mojo::StrongBinding<WebView> binding_;
  mojo::View* root_;
  mojo::View* content_;
  scoped_ptr<FrameTree> frame_tree_;

  // When LoadRequest() is called a PendingWebViewLoad is created to wait for
  // state needed to process the request. When the state is obtained OnLoad()
  // is invoked.
  scoped_ptr<PendingWebViewLoad> pending_load_;

  scoped_ptr<FrameDevToolsAgent> devtools_agent_;

  DISALLOW_COPY_AND_ASSIGN(WebViewImpl);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_
