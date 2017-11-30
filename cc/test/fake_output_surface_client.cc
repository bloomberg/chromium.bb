// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_output_surface_client.h"

namespace cc {

void FakeOutputSurfaceClient::DidReceiveSwapBuffersAck(uint64_t swap_id) {
  swap_count_++;
}

}  // namespace cc
