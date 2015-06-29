// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_tree.h"

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
            ViewOwnership::DOESNT_OWN_VIEW,
            root_client,
            user_data.Pass()) {
  root_.Init(nullptr);
}

FrameTree::~FrameTree() {
}

Frame* FrameTree::CreateAndAddFrame(mojo::View* view,
                                    Frame* parent,
                                    FrameTreeClient* client,
                                    scoped_ptr<FrameUserData> user_data) {
  Frame* frame =
      new Frame(this, view, ViewOwnership::OWNS_VIEW, client, user_data.Pass());
  frame->Init(parent);
  return frame;
}

Frame* FrameTree::CreateOrReplaceFrame(Frame* frame,
                                       mojo::View* view,
                                       FrameTreeClient* frame_tree_client,
                                       scoped_ptr<FrameUserData> user_data) {
  DCHECK(frame && frame->HasAncestor(&root_));

  Frame* parent = frame;
  if (frame->view() == view) {
    DCHECK(frame != &root_);
    // Rembed in existing frame.
    parent = frame->parent();
    delete frame;
    frame = nullptr;
  }  // else case is a view is becoming associated with a Frame, eg a new frame.

  DCHECK(parent);

  return CreateAndAddFrame(view, parent, frame_tree_client, user_data.Pass());
}

}  // namespace mandoline
