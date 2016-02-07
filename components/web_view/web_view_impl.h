// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_
#define COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_

#include <stdint.h>

#include <deque>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/web_view/find_controller.h"
#include "components/web_view/find_controller_delegate.h"
#include "components/web_view/frame_devtools_agent_delegate.h"
#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/navigation_controller.h"
#include "components/web_view/navigation_controller_delegate.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "components/web_view/public/interfaces/web_view.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mojo {
class Shell;
}

namespace web_view {

class Frame;
class FrameDevToolsAgent;
class FrameTree;
class PendingWebViewLoad;
class URLRequestCloneable;

namespace mojom {
class HTMLMessageEvent;
}

class WebViewImpl : public mojom::WebView,
                    public mus::WindowTreeDelegate,
                    public mus::WindowObserver,
                    public FrameTreeDelegate,
                    public FrameDevToolsAgentDelegate,
                    public NavigationControllerDelegate,
                    public FindControllerDelegate {
 public:
  WebViewImpl(mojo::Shell* shell,
              mojom::WebViewClientPtr client,
              mojo::InterfaceRequest<WebView> request);
  ~WebViewImpl() override;

 private:
  friend class PendingWebViewLoad;

  // See description above |pending_load_| for details.
  void OnLoad(const GURL& pending_url);

  // Flatten the frame tree in pre-order, depth first.
  void PreOrderDepthFirstTraverseTree(Frame* node, std::vector<Frame*>* output);

  // Overridden from WebView:
  void LoadRequest(mojo::URLRequestPtr request) override;
  void GetWindowTreeClient(mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
                               window_tree_client) override;
  void Find(const mojo::String& search_text, bool forward_direction) override;
  void StopFinding() override;
  void GoBack() override;
  void GoForward() override;

  // Overridden from mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // Overridden from mus::WindowObserver:
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(mus::Window* window) override;

  // Overridden from FrameTreeDelegate:
  scoped_ptr<FrameUserData> CreateUserDataForNewFrame(
      mojom::FrameClientPtr frame_client) override;
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  mojom::HTMLMessageEvent* event) override;
  void LoadingStateChanged(bool loading, double progress) override;
  void TitleChanged(const mojo::String& title) override;
  void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) override;
  void CanNavigateFrame(Frame* target,
                        mojo::URLRequestPtr request,
                        const CanNavigateFrameCallback& callback) override;
  void DidStartNavigation(Frame* frame) override;
  void DidCommitProvisionalLoad(Frame* frame) override;
  void DidNavigateLocally(Frame* source, const GURL& url) override;
  void DidDestroyFrame(Frame* frame) override;
  void OnFindInFrameCountUpdated(int32_t request_id,
                                 Frame* frame,
                                 int32_t count,
                                 bool final_update) override;
  void OnFindInPageSelectionUpdated(int32_t request_id,
                                    Frame* frame,
                                    int32_t active_match_ordinal) override;

  // Overridden from FrameDevToolsAgent::Delegate:
  void HandlePageNavigateRequest(const GURL& url) override;

  // Overridden from NavigationControllerDelegate:
  void OnNavigate(mojo::URLRequestPtr request) override;
  void OnDidNavigate() override;

  // Overridden from FindControllerDelegate:
  std::vector<Frame*> GetAllFrames() override;
  mojom::WebViewClient* GetWebViewClient() override;

  mojo::Shell* shell_;
  mojom::WebViewClientPtr client_;
  mojo::StrongBinding<WebView> binding_;
  mus::Window* root_;
  mus::Window* content_;
  // |find_controller_| is referenced by frame_tree_'s frames at destruction.
  FindController find_controller_;
  scoped_ptr<FrameTree> frame_tree_;

  // When LoadRequest() is called a PendingWebViewLoad is created to wait for
  // state needed to process the request. When the state is obtained OnLoad()
  // is invoked.
  scoped_ptr<PendingWebViewLoad> pending_load_;

  scoped_ptr<FrameDevToolsAgent> devtools_agent_;

  NavigationController navigation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebViewImpl);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_WEB_VIEW_IMPL_H_
