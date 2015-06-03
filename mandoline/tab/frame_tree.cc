// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_tree.h"

namespace mandoline {

FrameTree::FrameTree(mojo::View* view)
    : view_(view),
      root_(this, view, ViewOwnership::DOESNT_OWN_VIEW, nullptr, nullptr) {
}

FrameTree::~FrameTree() {
}

}  // namespace mandoline
