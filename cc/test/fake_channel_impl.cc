// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_events.h"
#include "cc/test/fake_channel_impl.h"

namespace cc {

FakeChannelImpl::FakeChannelImpl() {}

void FakeChannelImpl::SetAnimationEvents(
    std::unique_ptr<AnimationEvents> queue) {}

}  // namespace cc
