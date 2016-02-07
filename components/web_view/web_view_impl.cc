// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/web_view_impl.h"

#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/web_view/client_initiated_frame_connection.h"
#include "components/web_view/frame.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/frame_devtools_agent.h"
#include "components/web_view/frame_tree.h"
#include "components/web_view/navigation_entry.h"
#include "components/web_view/pending_web_view_load.h"
#include "components/web_view/url_request_cloneable.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/shell/public/cpp/shell.h"
#include "url/gurl.h"

namespace web_view {

using web_view::mojom::ButtonState;

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, public:

WebViewImpl::WebViewImpl(mojo::Shell* shell,
                         mojom::WebViewClientPtr client,
                         mojo::InterfaceRequest<mojom::WebView> request)
    : shell_(shell),
      client_(std::move(client)),
      binding_(this, std::move(request)),
      root_(nullptr),
      content_(nullptr),
      find_controller_(this),
      navigation_controller_(this) {
  devtools_agent_.reset(new FrameDevToolsAgent(shell_, this));
  OnDidNavigate();
}

WebViewImpl::~WebViewImpl() {
  if (content_)
    content_->RemoveObserver(this);
  if (root_) {
    root_->RemoveObserver(this);
    mus::ScopedWindowPtr::DeleteWindowOrWindowManager(root_);
  }
}

void WebViewImpl::OnLoad(const GURL& pending_url) {
  // Frames are uniqued based on the id of the associated Window. By creating a
  // new Window each time through we ensure the renderers get a clean id, rather
  // than one they may know about and try to incorrectly use.
  if (content_) {
    content_->Destroy();
    DCHECK(!content_);
  }

  client_->TopLevelNavigationStarted(pending_url.spec());

  content_ = root_->connection()->NewWindow();
  content_->SetBounds(gfx::Rect(root_->bounds().size()));
  root_->AddChild(content_);
  content_->SetVisible(true);
  content_->AddObserver(this);

  scoped_ptr<PendingWebViewLoad> pending_load(std::move(pending_load_));
  scoped_ptr<FrameConnection> frame_connection(
      pending_load->frame_connection());
  mus::mojom::WindowTreeClientPtr window_tree_client =
      frame_connection->GetWindowTreeClient();

  Frame::ClientPropertyMap client_properties;
  if (devtools_agent_) {
    devtools_service::DevToolsAgentPtr forward_agent;
    frame_connection->application_connection()->ConnectToService(
        &forward_agent);
    devtools_agent_->AttachFrame(std::move(forward_agent), &client_properties);
  }

  mojom::FrameClient* frame_client = frame_connection->frame_client();
  const uint32_t content_handler_id = frame_connection->GetContentHandlerID();
  frame_tree_.reset(
      new FrameTree(content_handler_id, content_, std::move(window_tree_client),
                    this, frame_client, std::move(frame_connection),
                    client_properties, pending_load->navigation_start_time()));
}

void WebViewImpl::PreOrderDepthFirstTraverseTree(Frame* node,
                                                 std::vector<Frame*>* output) {
  output->push_back(node);
  for (Frame* child : node->children())
    PreOrderDepthFirstTraverseTree(child, output);
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, WebView implementation:

void WebViewImpl::LoadRequest(mojo::URLRequestPtr request) {
  navigation_controller_.LoadURL(std::move(request));
}

void WebViewImpl::GetWindowTreeClient(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> window_tree_client) {
  mus::WindowTreeConnection::Create(
      this, std::move(window_tree_client),
      mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

void WebViewImpl::Find(const mojo::String& search_text,
                       bool forward_direction) {
  find_controller_.Find(search_text.To<std::string>(), forward_direction);
}

void WebViewImpl::StopFinding() {
  find_controller_.StopFinding();
}

void WebViewImpl::GoBack() {
  if (!navigation_controller_.CanGoBack())
    return;
  navigation_controller_.GoBack();
}

void WebViewImpl::GoForward() {
  if (!navigation_controller_.CanGoForward())
    return;
  navigation_controller_.GoForward();
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, mus::WindowTreeDelegate implementation:

void WebViewImpl::OnEmbed(mus::Window* root) {
  // We must have been granted embed root priviledges, otherwise we can't
  // Embed() in any descendants.
  DCHECK(root->connection()->IsEmbedRoot());
  root->AddObserver(this);
  root_ = root;

  if (pending_load_ && pending_load_->is_content_handler_id_valid())
    OnLoad(pending_load_->pending_url());
}

void WebViewImpl::OnConnectionLost(mus::WindowTreeConnection* connection) {
  root_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, mus::WindowObserver implementation:

void WebViewImpl::OnWindowBoundsChanged(mus::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  if (window != content_ && content_)
    content_->SetBounds(gfx::Rect(new_bounds.size()));
}

void WebViewImpl::OnWindowDestroyed(mus::Window* window) {
  // |FrameTree| cannot outlive the content window.
  if (window == content_) {
    frame_tree_.reset();
    content_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, FrameTreeDelegate implementation:

scoped_ptr<FrameUserData> WebViewImpl::CreateUserDataForNewFrame(
    mojom::FrameClientPtr frame_client) {
  return make_scoped_ptr(
      new ClientInitiatedFrameConnection(std::move(frame_client)));
}

bool WebViewImpl::CanPostMessageEventToFrame(const Frame* source,
                                             const Frame* target,
                                             mojom::HTMLMessageEvent* event) {
  return true;
}

void WebViewImpl::LoadingStateChanged(bool loading, double progress) {
  client_->LoadingStateChanged(loading, progress);
}

void WebViewImpl::TitleChanged(const mojo::String& title) {
  client_->TitleChanged(title);
}

void WebViewImpl::NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) {
  client_->TopLevelNavigateRequest(std::move(request));
}

void WebViewImpl::CanNavigateFrame(Frame* target,
                                   mojo::URLRequestPtr request,
                                   const CanNavigateFrameCallback& callback) {
  FrameConnection::CreateConnectionForCanNavigateFrame(
      shell_, target, std::move(request), callback);
}

void WebViewImpl::DidStartNavigation(Frame* frame) {}

void WebViewImpl::DidCommitProvisionalLoad(Frame* frame) {
  navigation_controller_.FrameDidCommitProvisionalLoad(frame);
}

void WebViewImpl::DidNavigateLocally(Frame* source,
                                     const GURL& url) {
  navigation_controller_.FrameDidNavigateLocally(source, url);
  if (source == frame_tree_->root())
    client_->TopLevelNavigationStarted(url.spec());
}

void WebViewImpl::DidDestroyFrame(Frame* frame) {
  find_controller_.DidDestroyFrame(frame);
}

void WebViewImpl::OnFindInFrameCountUpdated(int32_t request_id,
                                            Frame* frame,
                                            int32_t count,
                                            bool final_update) {
  find_controller_.OnFindInFrameCountUpdated(request_id, frame, count,
                                             final_update);
}

void WebViewImpl::OnFindInPageSelectionUpdated(int32_t request_id,
                                               Frame* frame,
                                               int32_t active_match_ordinal) {
  find_controller_.OnFindInPageSelectionUpdated(request_id, frame,
                                                active_match_ordinal);
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, FrameDevToolsAgentDelegate implementation:

void WebViewImpl::HandlePageNavigateRequest(const GURL& url) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url.spec();
  client_->TopLevelNavigateRequest(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, NavigationControllerDelegate implementation:

void WebViewImpl::OnNavigate(mojo::URLRequestPtr request) {
  pending_load_.reset(new PendingWebViewLoad(this));
  pending_load_->Init(std::move(request));
}

void WebViewImpl::OnDidNavigate() {
  client_->BackForwardChanged(
      navigation_controller_.CanGoBack() ? ButtonState::ENABLED
                                         : ButtonState::DISABLED,
      navigation_controller_.CanGoForward() ? ButtonState::ENABLED
                                            : ButtonState::DISABLED);
}

////////////////////////////////////////////////////////////////////////////////
// WebViewImpl, FindControllerDelegate implementation:

std::vector<Frame*> WebViewImpl::GetAllFrames() {
  std::vector<Frame*> all_frames;
  PreOrderDepthFirstTraverseTree(frame_tree_->root(), &all_frames);
  return all_frames;
}

mojom::WebViewClient* WebViewImpl::GetWebViewClient() {
  return client_.get();
}

}  // namespace web_view
