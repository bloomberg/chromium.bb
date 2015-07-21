// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_TREE_H_
#define MANDOLINE_TAB_FRAME_TREE_H_

#include "mandoline/tab/frame.h"

namespace mandoline {

class FrameTreeClient;
class FrameTreeDelegate;
class FrameUserData;

// FrameTree manages the set of Frames that comprise a single url. FrameTree
// owns the root Frame and each Frame owns its children. Frames are
// automatically deleted and removed from the tree if the corresponding view is
// deleted. This happens if the creator of the view deletes it (say an iframe is
// destroyed).
class FrameTree {
 public:
  // |view| is the view to do the initial embedding in. It is assumed |view|
  // outlives FrameTree.
  FrameTree(mojo::View* view,
            FrameTreeDelegate* delegate,
            FrameTreeClient* root_client,
            scoped_ptr<FrameUserData> user_data);
  ~FrameTree();

  Frame* root() { return &root_; }

  Frame* CreateAndAddFrame(mojo::View* view,
                           Frame* parent,
                           FrameTreeClient* client,
                           scoped_ptr<FrameUserData> user_data);

  // If frame->view() == |view|, then |frame| is deleted and a new Frame created
  // to replace it. Otherwise a new Frame is created as a child of |frame|.
  // It is expected this is called from
  //  ViewManagerDelegate::OnEmbedForDescendant().
  Frame* CreateOrReplaceFrame(Frame* frame,
                              mojo::View* view,
                              FrameTreeClient* frame_tree_client,
                              scoped_ptr<FrameUserData> user_data);

 private:
  friend class Frame;

  void LoadingStateChanged();
  void ProgressChanged();
  void FrameNameChanged(Frame* frame);

  mojo::View* view_;

  FrameTreeDelegate* delegate_;

  Frame root_;

  double progress_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_TREE_H_
