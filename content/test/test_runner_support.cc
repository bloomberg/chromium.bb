// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_runner_support.h"

#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

std::string GetUniqueNameForFrame(blink::WebLocalFrame* frame) {
  return RenderFrameImpl::FromWebFrame(frame)->unique_name();
}

}  // namespace content
