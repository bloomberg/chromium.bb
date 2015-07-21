// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame_tree_manager.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "components/html_viewer/blink_basic_type_converters.h"
#include "components/html_viewer/blink_url_request_type_converters.h"
#include "components/html_viewer/global_state.h"
#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_frame_tree_manager_delegate.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size.h"

namespace html_viewer {
namespace {

mojo::Target WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return mojo::TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return mojo::TARGET_NEW_NODE;
    default:
      return mojo::TARGET_DEFAULT;
  }
}

bool CanNavigateLocally(blink::WebFrame* frame,
                        const blink::WebURLRequest& request) {
  // If we have extraData() it means we already have the url response
  // (presumably because we are being called via Navigate()). In that case we
  // can go ahead and navigate locally.
  if (request.extraData())
    return true;

  // Otherwise we don't know if we're the right app to handle this request. Ask
  // host to do the navigation for us.
  return false;
}

// Creates a Frame per FrameData element in |frame_data|.
HTMLFrame* BuildFrameTree(
    HTMLFrameTreeManager* frame_tree_manager,
    const mojo::Array<mandoline::FrameDataPtr>& frame_data,
    uint32_t local_frame_id,
    mojo::View* local_view) {
  std::vector<HTMLFrame*> parents;
  HTMLFrame* root = nullptr;
  HTMLFrame* last_frame = nullptr;
  for (size_t i = 0; i < frame_data.size(); ++i) {
    if (last_frame && frame_data[i]->parent_id == last_frame->id()) {
      parents.push_back(last_frame);
    } else if (!parents.empty()) {
      while (parents.back()->id() != frame_data[i]->parent_id)
        parents.pop_back();
    }
    HTMLFrame::CreateParams params(frame_tree_manager,
                                   !parents.empty() ? parents.back() : nullptr,
                                   frame_data[i]->frame_id);
    HTMLFrame* frame = new HTMLFrame(params);
    if (!last_frame)
      root = frame;
    else
      DCHECK(frame->parent());
    last_frame = frame;

    frame->Init(local_view, frame_data[i]->name.To<blink::WebString>(),
                frame_data[i]->origin.To<blink::WebString>());
  }
  return root;
}

}  // namespace

HTMLFrameTreeManager::HTMLFrameTreeManager(
    GlobalState* global_state,
    mojo::ApplicationImpl* app,
    mojo::ApplicationConnection* app_connection,
    uint32_t local_frame_id,
    mandoline::FrameTreeServerPtr server)
    : global_state_(global_state),
      app_(app),
      delegate_(nullptr),
      local_frame_id_(local_frame_id),
      server_(server.Pass()),
      navigator_host_(app_connection->GetServiceProvider()),
      root_(nullptr) {}

HTMLFrameTreeManager::~HTMLFrameTreeManager() {
  if (root_)
    root_->Close();  // This should call back to OnFrameDestroyed().
  DCHECK(!root_);
}

void HTMLFrameTreeManager::Init(
    mojo::View* local_view,
    mojo::Array<mandoline::FrameDataPtr> frame_data) {
  root_ = BuildFrameTree(this, frame_data, local_frame_id_, local_view);
  HTMLFrame* local_frame = root_->FindFrame(local_frame_id_);
  CHECK(local_frame);
  local_frame->UpdateFocus();
}

HTMLFrame* HTMLFrameTreeManager::GetLocalFrame() {
  return root_->FindFrame(local_frame_id_);
}

blink::WebLocalFrame* HTMLFrameTreeManager::GetLocalWebFrame() {
  return GetLocalFrame()->web_frame()->toWebLocalFrame();
}

blink::WebView* HTMLFrameTreeManager::GetWebView() {
  return root_->web_view();
}

blink::WebNavigationPolicy HTMLFrameTreeManager::DecidePolicyForNavigation(
    HTMLFrame* frame,
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  if (info.frame == frame->web_frame() && frame == root_ && delegate_ &&
      delegate_->ShouldNavigateLocallyInMainFrame()) {
    return info.defaultPolicy;
  }

  if (CanNavigateLocally(info.frame, info.urlRequest))
    return info.defaultPolicy;

  // TODO(sky): this is wrong for subframes. In fact NavigatorHost should likely
  // be merged with Frame.
  if (navigator_host_.get()) {
    mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
    navigator_host_->RequestNavigate(
        WebNavigationPolicyToNavigationTarget(info.defaultPolicy),
        url_request.Pass());
  }

  return blink::WebNavigationPolicyIgnore;
}

void HTMLFrameTreeManager::OnFrameDidFinishLoad(HTMLFrame* frame) {
  if (delegate_)
    delegate_->OnFrameDidFinishLoad(frame);
}

void HTMLFrameTreeManager::OnFrameDidNavigateLocally(HTMLFrame* frame,
                                                     const std::string& url) {
  if (navigator_host_.get() && frame == root_)
    navigator_host_->DidNavigateLocally(url);
}

void HTMLFrameTreeManager::OnFrameDestroyed(HTMLFrame* frame) {
  if (frame == root_) {
    root_ = nullptr;
    // Shortly after this HTMLDocumentOOPIF should get ViewManagerDestroyed()
    // and delete us.
  }
}

void HTMLFrameTreeManager::OnFrameDidChangeName(HTMLFrame* frame,
                                                const blink::WebString& name) {
  if (frame != GetLocalFrame())
    return;

  mojo::String mojo_name;
  if (!name.isNull())
    mojo_name = name.utf8();
  server_->SetFrameName(mojo_name);
}

void HTMLFrameTreeManager::OnConnect(
    mandoline::FrameTreeServerPtr server,
    mojo::Array<mandoline::FrameDataPtr> frame_data) {
  // OnConnection() is only sent once, and has been received (by
  // DocumentResourceWaiter) by the time we get here.
  NOTREACHED();
}

void HTMLFrameTreeManager::LoadingStarted() {
  server_->LoadingStarted();
}

void HTMLFrameTreeManager::LoadingStopped() {
  server_->LoadingStopped();
}

void HTMLFrameTreeManager::ProgressChanged(double progress) {
  server_->ProgressChanged(progress);
}

void HTMLFrameTreeManager::OnFrameAdded(mandoline::FrameDataPtr frame_data) {
  NOTIMPLEMENTED();
}

void HTMLFrameTreeManager::OnFrameRemoved(uint32_t frame_id) {
  NOTIMPLEMENTED();
}

void HTMLFrameTreeManager::OnFrameNameChanged(uint32_t frame_id,
                                              const mojo::String& name) {
  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (frame)
    frame->SetRemoteFrameName(name);
}

}  // namespace mojo
