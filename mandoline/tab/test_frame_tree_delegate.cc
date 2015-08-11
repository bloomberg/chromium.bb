// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/test_frame_tree_delegate.h"

namespace mandoline {

TestFrameTreeDelegate::TestFrameTreeDelegate() {}

TestFrameTreeDelegate::~TestFrameTreeDelegate() {}

bool TestFrameTreeDelegate::CanPostMessageEventToFrame(
    const Frame* source,
    const Frame* target,
    HTMLMessageEvent* event) {
  return true;
}

void TestFrameTreeDelegate::LoadingStateChanged(bool loading) {}

void TestFrameTreeDelegate::ProgressChanged(double progress) {}

void TestFrameTreeDelegate::RequestNavigate(Frame* source,
                                            NavigationTargetType target_type,
                                            Frame* target_frame,
                                            mojo::URLRequestPtr request) {}

}  // namespace mandoline
