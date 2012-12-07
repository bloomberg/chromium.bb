// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl.h"

namespace cc {

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(Proxy* proxy)
    : LayerTreeHostImpl(settings_, &client_, proxy)
{
    // Explicitly clear all debug settings.
    setDebugState(LayerTreeDebugState());
}

FakeLayerTreeHostImpl::~FakeLayerTreeHostImpl()
{
}

}  // namespace cc
