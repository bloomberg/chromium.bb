// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_RUNNER_SUPPORT_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_RUNNER_SUPPORT_H_

#include <string>

namespace blink {
class WebLocalFrame;
}

namespace content {

// Returns a frame name that can be used in the output of layout tests
// (the name is derived from the frame's unique name).
std::string GetFrameNameForLayoutTests(blink::WebLocalFrame* frame);

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_RUNNER_SUPPORT_H_
