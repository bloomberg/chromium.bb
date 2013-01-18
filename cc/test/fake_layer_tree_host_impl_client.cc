// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl_client.h"

namespace cc {

bool FakeLayerTreeHostImplClient::reduceContentsTextureMemoryOnImplThread(
    size_t,
    int) {
  return false;
}

bool FakeLayerTreeHostImplClient::isInsideDraw() {
  return false;
}

}  // namespace cc
