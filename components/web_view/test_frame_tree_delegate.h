// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/web_view/frame_tree_delegate.h"

namespace base {
class RunLoop;
}

namespace mojo {
class Shell;
}

namespace web_view {

class TestFrameTreeDelegate : public FrameTreeDelegate {
 public:
  explicit TestFrameTreeDelegate(mojo::Shell* shell);
  ~TestFrameTreeDelegate() override;

  mojo::Shell* shell() { return shell_; }

  // Runs a message loop until DidCreateFrame() is called, returning the
  // Frame supplied to DidCreateFrame().
  Frame* WaitForCreateFrame();

  // Waits for DidDestroyFrame() to be called with |frame|.
  void WaitForDestroyFrame(Frame* frame);

  // Waits for OnWindowEmbeddedInFrameDisconnected() to be called with |frame|.
  void WaitForFrameDisconnected(Frame* frame);

  // TestFrameTreeDelegate:
  scoped_ptr<FrameUserData> CreateUserDataForNewFrame(
      mojom::FrameClientPtr frame_client) override;
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  mojom::HTMLMessageEvent* event) override;
  void LoadingStateChanged(bool loading, double progress) override;
  void TitleChanged(const mojo::String& title) override;
  void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) override;
  void CanNavigateFrame(Frame* target,
                        mojo::URLRequestPtr request,
                        const CanNavigateFrameCallback& callback) override;
  void DidStartNavigation(Frame* frame) override;
  void DidCommitProvisionalLoad(Frame* frame) override;
  void DidNavigateLocally(Frame* source, const GURL& url) override;
  void DidCreateFrame(Frame* frame) override;
  void DidDestroyFrame(Frame* frame) override;
  void OnWindowEmbeddedInFrameDisconnected(Frame* frame) override;

 private:
  bool is_waiting() const { return run_loop_.get(); }

  mojo::Shell* shell_;
  bool waiting_for_create_frame_;
  Frame* waiting_for_destroy_frame_;
  scoped_ptr<base::RunLoop> run_loop_;
  Frame* most_recent_frame_;
  Frame* waiting_for_frame_disconnected_;

  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeDelegate);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_
