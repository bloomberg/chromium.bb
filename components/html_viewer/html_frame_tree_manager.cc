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

mandoline::NavigationTarget WebNavigationPolicyToNavigationTarget(
    blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyCurrentTab:
      return mandoline::NAVIGATION_TARGET_SOURCE_NODE;
    case blink::WebNavigationPolicyNewBackgroundTab:
    case blink::WebNavigationPolicyNewForegroundTab:
    case blink::WebNavigationPolicyNewWindow:
    case blink::WebNavigationPolicyNewPopup:
      return mandoline::NAVIGATION_TARGET_NEW_NODE;
    default:
      return mandoline::NAVIGATION_TARGET_DEFAULT;
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

  mojo::URLRequestPtr url_request = mojo::URLRequest::From(info.urlRequest);
  server_->RequestNavigate(
      frame->id(), WebNavigationPolicyToNavigationTarget(info.defaultPolicy),
      url_request.Pass());

  return blink::WebNavigationPolicyIgnore;
}

void HTMLFrameTreeManager::OnFrameDidFinishLoad(HTMLFrame* frame) {
  if (delegate_)
    delegate_->OnFrameDidFinishLoad(frame);
}

void HTMLFrameTreeManager::OnFrameDidNavigateLocally(HTMLFrame* frame,
                                                     const std::string& url) {
  server_->DidNavigateLocally(frame->id(), url);
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
  DCHECK(frame->IsLocal());
  mojo::String mojo_name;
  if (!name.isNull())
    mojo_name = name.utf8();
  server_->SetFrameName(frame->id(), mojo_name);
}

void HTMLFrameTreeManager::OnConnect(
    mandoline::FrameTreeServerPtr server,
    mojo::Array<mandoline::FrameDataPtr> frame_data) {
  // OnConnection() is only sent once, and has been received (by
  // DocumentResourceWaiter) by the time we get here.
  NOTREACHED();
}

void HTMLFrameTreeManager::LoadingStarted(HTMLFrame* frame) {
  DCHECK(frame->IsLocal());
  server_->LoadingStarted(frame->id());
}

void HTMLFrameTreeManager::LoadingStopped(HTMLFrame* frame) {
  DCHECK(frame->IsLocal());
  server_->LoadingStopped(frame->id());
}

void HTMLFrameTreeManager::ProgressChanged(HTMLFrame* frame, double progress) {
  DCHECK(frame->IsLocal());
  server_->ProgressChanged(frame->id(), progress);
}

void HTMLFrameTreeManager::OnFrameAdded(mandoline::FrameDataPtr frame_data) {
  HTMLFrame* parent = root_->FindFrame(frame_data->parent_id);
  if (!parent) {
    DVLOG(1) << "Received invalid parent in OnFrameAdded "
             << frame_data->parent_id;
    return;
  }
  if (root_->FindFrame(frame_data->frame_id)) {
    DVLOG(1) << "Child with id already exists in OnFrameAdded "
             << frame_data->frame_id;
    return;
  }

  HTMLFrame::CreateParams params(this, parent, frame_data->frame_id);
  // |parent| takes ownership of |frame|.
  HTMLFrame* frame = new HTMLFrame(params);
  frame->Init(nullptr, frame_data->name.To<blink::WebString>(),
              frame_data->origin.To<blink::WebString>());
}

void HTMLFrameTreeManager::OnFrameRemoved(uint32_t frame_id) {
  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (!frame) {
    DVLOG(1) << "OnFrameRemoved with unknown frame " << frame_id;
    return;
  }

  // We shouldn't see requests to remove the root.
  if (frame == root_) {
    DVLOG(1) << "OnFrameRemoved supplied root; ignoring";
    return;
  }

  // Requests to remove the local frame are followed by the View being
  // destroyed. We handle destruction there.
  if (frame == GetLocalFrame())
    return;

  frame->Close();
}

void HTMLFrameTreeManager::OnFrameNameChanged(uint32_t frame_id,
                                              const mojo::String& name) {
  HTMLFrame* frame = root_->FindFrame(frame_id);
  if (frame)
    frame->SetRemoteFrameName(name);
}

}  // namespace mojo
