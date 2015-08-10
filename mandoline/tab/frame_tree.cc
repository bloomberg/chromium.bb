// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_tree.h"

#include "mandoline/tab/frame_tree_delegate.h"
#include "mandoline/tab/frame_user_data.h"

namespace mandoline {

FrameTree::FrameTree(mojo::View* view,
                     FrameTreeDelegate* delegate,
                     FrameTreeClient* root_client,
                     scoped_ptr<FrameUserData> user_data)
    : view_(view),
      delegate_(delegate),
      root_(this,
            view,
            view->id(),
            ViewOwnership::DOESNT_OWN_VIEW,
            root_client,
            user_data.Pass(),
            Frame::ClientPropertyMap()),
      progress_(0.f),
      change_id_(1u) {
  root_.Init(nullptr);
}

FrameTree::~FrameTree() {
}

Frame* FrameTree::CreateAndAddFrame(mojo::View* view,
                                    Frame* parent,
                                    FrameTreeClient* client,
                                    scoped_ptr<FrameUserData> user_data) {
  return CreateAndAddFrameImpl(view, view->id(), parent, client,
                               user_data.Pass(), Frame::ClientPropertyMap());
}

Frame* FrameTree::CreateOrReplaceFrame(Frame* frame,
                                       mojo::View* view,
                                       FrameTreeClient* frame_tree_client,
                                       scoped_ptr<FrameUserData> user_data) {
  DCHECK(frame && frame->HasAncestor(&root_));

  if (frame->view() == view) {
    // It's important we Swap() here rather than destroy as otherwise the
    // clients see a destroy followed by a new frame, which confuses html
    // viewer.
    DCHECK(frame != &root_);
    frame->Swap(frame_tree_client, user_data.Pass());
    return frame;
  }

  return CreateAndAddFrame(view, frame, frame_tree_client, user_data.Pass());
}

void FrameTree::CreateSharedFrame(
    Frame* parent,
    uint32_t frame_id,
    const Frame::ClientPropertyMap& client_properties) {
  mojo::View* frame_view = root_.view()->GetChildById(frame_id);
  // |frame_view| may be null if the View hasn't been created yet. If this is
  // the case the View will be connected to the Frame in Frame::OnTreeChanged.
  CreateAndAddFrameImpl(frame_view, frame_id, parent, nullptr, nullptr,
                        client_properties);
}

uint32_t FrameTree::AdvanceChangeID() {
  return ++change_id_;
}

Frame* FrameTree::CreateAndAddFrameImpl(
    mojo::View* view,
    uint32_t frame_id,
    Frame* parent,
    FrameTreeClient* client,
    scoped_ptr<FrameUserData> user_data,
    const Frame::ClientPropertyMap& client_properties) {
  Frame* frame = new Frame(this, view, frame_id, ViewOwnership::OWNS_VIEW,
                           client, user_data.Pass(), client_properties);
  frame->Init(parent);
  return frame;
}

void FrameTree::LoadingStateChanged() {
  bool loading = root_.IsLoading();
  if (delegate_)
    delegate_->LoadingStateChanged(loading);
}

void FrameTree::ProgressChanged() {
  int frame_count = 0;
  double total_progress = root_.GatherProgress(&frame_count);
  // Make sure the progress bar never runs backwards, even if that means
  // accuracy takes a hit.
  progress_ = std::max(progress_, total_progress / frame_count);
  if (delegate_)
    delegate_->ProgressChanged(progress_);
}

void FrameTree::ClientPropertyChanged(const Frame* source,
                                      const mojo::String& name,
                                      const mojo::Array<uint8_t>& value) {
  root_.NotifyClientPropertyChanged(source, name, value);
}

}  // namespace mandoline
