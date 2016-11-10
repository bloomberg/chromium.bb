// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_channel_impl.h"
#include "cc/trees/mutator_host.h"

namespace cc {

FakeChannelImpl::FakeChannelImpl() {}

void FakeChannelImpl::SetAnimationEvents(std::unique_ptr<MutatorEvents> queue) {
}

}  // namespace cc
