// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_FRAME_OBSERVER_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "components/test_runner/layout_dump_flags.h"
#include "content/public/renderer/render_frame_observer.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {
class RenderFrame;

class LayoutTestRenderFrameObserver : public RenderFrameObserver {
 public:
  explicit LayoutTestRenderFrameObserver(RenderFrame* render_frame);
  ~LayoutTestRenderFrameObserver() override {}

  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnLayoutDumpRequest(test_runner::LayoutDumpFlags layout_dump_flags);

  DISALLOW_COPY_AND_ASSIGN(LayoutTestRenderFrameObserver);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_LAYOUT_TEST_RENDER_FRAME_OBSERVER_H_
