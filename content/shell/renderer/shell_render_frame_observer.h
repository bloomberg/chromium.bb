// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_SHELL_RENDER_FRAME_OBSERVER_H_
#define CONTENT_SHELL_SHELL_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame_observer.h"

namespace blink {
class WebFrame;
}

namespace content {
class RenderFrame;

class ShellRenderFrameObserver : public RenderFrameObserver {
 public:
  explicit ShellRenderFrameObserver(RenderFrame* render_frame);
  virtual ~ShellRenderFrameObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRenderFrameObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_SHELL_RENDER_FRAME_OBSERVER_H_
