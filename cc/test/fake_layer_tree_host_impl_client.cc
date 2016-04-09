// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_events.h"
#include "cc/test/fake_layer_tree_host_impl_client.h"

namespace cc {

bool FakeLayerTreeHostImplClient::IsInsideDraw() {
  return false;
}

void FakeLayerTreeHostImplClient::PostAnimationEventsToMainThreadOnImplThread(
    std::unique_ptr<AnimationEvents> events) {}

}  // namespace cc
