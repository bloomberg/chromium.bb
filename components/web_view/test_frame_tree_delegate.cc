// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/test_frame_tree_delegate.h"

namespace web_view {

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

void TestFrameTreeDelegate::NavigateTopLevel(Frame* source,
                                             mojo::URLRequestPtr request) {}

void TestFrameTreeDelegate::CanNavigateFrame(
    Frame* target,
    mojo::URLRequestPtr request,
    const CanNavigateFrameCallback& callback) {}

void TestFrameTreeDelegate::DidStartNavigation(Frame* frame) {}

}  // namespace web_view
