// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame_tree.h"

#include "components/web_view/frame_tree_delegate.h"
#include "components/web_view/frame_user_data.h"

namespace web_view {

FrameTree::FrameTree(uint32_t root_app_id,
                     mus::Window* window,
                     mus::mojom::WindowTreeClientPtr window_tree_client,
                     FrameTreeDelegate* delegate,
                     mojom::FrameClient* root_client,
                     scoped_ptr<FrameUserData> user_data,
                     const Frame::ClientPropertyMap& client_properties,
                     base::TimeTicks navigation_start_time)
    : window_(window),
      delegate_(delegate),
      root_(new Frame(this,
                      window,
                      window->id(),
                      root_app_id,
                      WindowOwnership::DOESNT_OWN_WINDOW,
                      root_client,
                      user_data.Pass(),
                      client_properties)),
      progress_(0.f),
      change_id_(1u) {
  root_->Init(nullptr, window_tree_client.Pass(), nullptr,
              navigation_start_time);
}

FrameTree::~FrameTree() {
  // Destroy the root explicitly in case it calls back to us for state (such
  // as to see if it is the root).
  delete root_;
  root_ = nullptr;
}

Frame* FrameTree::CreateChildFrame(
    Frame* parent,
    mojo::InterfaceRequest<mojom::Frame> frame_request,
    mojom::FrameClientPtr client,
    uint32_t frame_id,
    uint32_t app_id,
    const Frame::ClientPropertyMap& client_properties) {
  mojom::FrameClient* raw_client = client.get();
  scoped_ptr<FrameUserData> user_data =
      delegate_->CreateUserDataForNewFrame(client.Pass());
  mus::Window* frame_window = root_->window()->GetChildById(frame_id);
  // |frame_window| may be null if the Window hasn't been created yet. If this
  // is the case the Window will be connected to the Frame in
  // Frame::OnTreeChanged.
  Frame* frame = new Frame(this, frame_window, frame_id, app_id,
                           WindowOwnership::OWNS_WINDOW, raw_client,
                           user_data.Pass(), client_properties);
  frame->Init(parent, nullptr, frame_request.Pass(), base::TimeTicks());
  return frame;
}

uint32_t FrameTree::AdvanceChangeID() {
  return ++change_id_;
}

void FrameTree::LoadingStateChanged() {
  const bool loading = root_->IsLoading();
  if (loading) {
    int frame_count = 0;
    const double total_progress = root_->GatherProgress(&frame_count);
    // Make sure the progress bar never runs backwards, even if that means
    // accuracy takes a hit.
    progress_ = std::max(progress_, total_progress / frame_count);
  }
  delegate_->LoadingStateChanged(loading, progress_);
}

void FrameTree::TitleChanged(const mojo::String& title) {
  delegate_->TitleChanged(title);
}

void FrameTree::DidCommitProvisionalLoad(Frame* source) {
  delegate_->DidCommitProvisionalLoad(source);
}

void FrameTree::DidNavigateLocally(Frame* source, const GURL& url) {
  delegate_->DidNavigateLocally(source, url);
}

void FrameTree::ClientPropertyChanged(const Frame* source,
                                      const mojo::String& name,
                                      const mojo::Array<uint8_t>& value) {
  root_->NotifyClientPropertyChanged(source, name, value);
}

}  // namespace web_view
