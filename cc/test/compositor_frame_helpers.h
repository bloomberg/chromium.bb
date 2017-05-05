// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_COMPOSITOR_FRAME_HELPERS_H_
#define CC_TEST_COMPOSITOR_FRAME_HELPERS_H_

namespace cc {

class CompositorFrame;

namespace test {

// Creates a valid CompositorFrame.
CompositorFrame MakeCompositorFrame();

// Creates a CompositorFrame that will be valid once its render_pass_list is
// initialized.
CompositorFrame MakeEmptyCompositorFrame();

}  // namespace test
}  // namespace cc

#endif  // CC_TEST_COMPOSITOR_FRAME_HELPERS_H_
