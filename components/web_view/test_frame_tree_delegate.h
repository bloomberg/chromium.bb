// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_
#define COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_

#include "base/basictypes.h"
#include "components/web_view/frame_tree_delegate.h"

namespace web_view {

class TestFrameTreeDelegate : public FrameTreeDelegate {
 public:
  TestFrameTreeDelegate();
  ~TestFrameTreeDelegate() override;

  // TestFrameTreeDelegate:
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  HTMLMessageEvent* event) override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;
  void TitleChanged(const mojo::String& title) override;
  void NavigateTopLevel(Frame* source, mojo::URLRequestPtr request) override;
  void CanNavigateFrame(Frame* target,
                        mojo::URLRequestPtr request,
                        const CanNavigateFrameCallback& callback) override;
  void DidStartNavigation(Frame* frame) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeDelegate);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_TEST_FRAME_TREE_DELEGATE_H_
