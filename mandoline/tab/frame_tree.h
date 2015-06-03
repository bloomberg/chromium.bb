// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_TREE_H_
#define MANDOLINE_TAB_FRAME_TREE_H_

#include "mandoline/tab/frame.h"

namespace mandoline {

// FrameTree manages the set of Frames that comprise a single url. FrameTree
// owns the root Frame and each Frames owns its children. Frames are
// automatically deleted and removed from the tree if the corresponding view is
// deleted. This happens if the creator of the view deletes it (say an iframe is
// destroyed).
class FrameTree {
 public:
  // |view| is the view to do the initial embedding in. It is assumed |view|
  // outlives FrameTree.
  explicit FrameTree(mojo::View* view);
  ~FrameTree();

  Frame* root() { return &root_; }

 private:
  mojo::View* view_;

  Frame root_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_TREE_H_
