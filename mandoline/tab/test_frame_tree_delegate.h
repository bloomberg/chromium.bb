// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_TEST_FRAME_TREE_DELEGATE_H_
#define MANDOLINE_TAB_TEST_FRAME_TREE_DELEGATE_H_

#include "base/basictypes.h"
#include "mandoline/tab/frame_tree_delegate.h"

namespace mandoline {

class TestFrameTreeDelegate : public FrameTreeDelegate {
 public:
  TestFrameTreeDelegate();
  ~TestFrameTreeDelegate() override;

  // TestFrameTreeDelegate:
  bool CanPostMessageEventToFrame(const Frame* source,
                                  const Frame* target,
                                  MessageEvent* event) override;
  void LoadingStateChanged(bool loading) override;
  void ProgressChanged(double progress) override;
  void RequestNavigate(Frame* source,
                       NavigationTargetType target_type,
                       Frame* target_frame,
                       mojo::URLRequestPtr request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFrameTreeDelegate);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_TEST_FRAME_TREE_DELEGATE_H_
