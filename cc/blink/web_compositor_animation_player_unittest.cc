// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_player.h"
#include "cc/blink/web_compositor_animation_player_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCompositorAnimationDelegate.h"

using base::TimeTicks;
using blink::WebCompositorAnimationDelegate;
using cc::Animation;
using testing::_;

namespace cc_blink {
namespace {

class MockWebCompositorAnimationDelegate
    : public WebCompositorAnimationDelegate {
 public:
  MockWebCompositorAnimationDelegate() {}

  MOCK_METHOD2(notifyAnimationStarted, void(double, int));
  MOCK_METHOD2(notifyAnimationFinished, void(double, int));
  MOCK_METHOD2(notifyAnimationAborted, void(double, int));
};

// Test that when the animation delegate is null, the animation player
// doesn't forward the finish notification.
TEST(WebCompositorAnimationPlayerTest, NullDelegate) {
  scoped_ptr<WebCompositorAnimationDelegate> delegate(
      new MockWebCompositorAnimationDelegate);
  EXPECT_CALL(*static_cast<MockWebCompositorAnimationDelegate*>(delegate.get()),
              notifyAnimationFinished(_, _))
      .Times(1);

  scoped_ptr<WebCompositorAnimationPlayerImpl> web_player(
      new WebCompositorAnimationPlayerImpl);
  cc::AnimationPlayer* player = web_player->animation_player();

  web_player->setAnimationDelegate(delegate.get());
  player->NotifyAnimationFinished(TimeTicks(), Animation::SCROLL_OFFSET, 0);

  web_player->setAnimationDelegate(nullptr);
  player->NotifyAnimationFinished(TimeTicks(), Animation::SCROLL_OFFSET, 0);
}

}  // namespace
}  // namespace cc_blink
