// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CHANNEL_IMPL_H_
#define CC_TEST_FAKE_CHANNEL_IMPL_H_

#include "base/macros.h"
#include "cc/trees/channel_impl.h"

namespace cc {

class FakeChannelImpl : public ChannelImpl {
 public:
  FakeChannelImpl();

  ~FakeChannelImpl() override {}

  void DidCompleteSwapBuffers() override {}
  void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void DidCommitAndDrawFrame() override {}
  void SetAnimationEvents(std::unique_ptr<AnimationEvents> queue) override;
  void DidLoseOutputSurface() override {}
  void RequestNewOutputSurface() override {}
  void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) override {}
  void DidCompletePageScaleAnimation() override {}
  void BeginMainFrame(std::unique_ptr<BeginMainFrameAndCommitState>
                          begin_main_frame_state) override {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CHANNEL_IMPL_H_
