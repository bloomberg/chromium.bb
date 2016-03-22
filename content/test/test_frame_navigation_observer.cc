// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_frame_navigation_observer.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestFrameNavigationObserver::TestFrameNavigationObserver(
    FrameTreeNode* node)
    : WebContentsObserver(
          node->current_frame_host()->delegate()->GetAsWebContents()),
      frame_tree_node_id_(node->frame_tree_node_id()),
      navigation_started_(false),
      has_committed_(false),
      wait_for_commit_(false),
      message_loop_runner_(new MessageLoopRunner) {
}

TestFrameNavigationObserver::~TestFrameNavigationObserver() {
}

void TestFrameNavigationObserver::Wait() {
  wait_for_commit_ = false;
  message_loop_runner_->Run();
}

void TestFrameNavigationObserver::WaitForCommit() {
  if (has_committed_)
    return;

  wait_for_commit_ = true;
  message_loop_runner_->Run();
}

void TestFrameNavigationObserver::DidStartProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (rfh->frame_tree_node()->frame_tree_node_id() == frame_tree_node_id_) {
    navigation_started_ = true;
    has_committed_ = false;
  }
}

void TestFrameNavigationObserver::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (!navigation_started_)
    return;

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  if (rfh->frame_tree_node()->frame_tree_node_id() != frame_tree_node_id_)
    return;

  has_committed_ = true;
  if (wait_for_commit_)
    message_loop_runner_->Quit();
}

void TestFrameNavigationObserver::DidStopLoading() {
  if (!navigation_started_)
    return;

  navigation_started_ = false;
  message_loop_runner_->Quit();
}

}  // namespace content
