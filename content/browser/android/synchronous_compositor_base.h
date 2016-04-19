// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BASE_H_
#define CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BASE_H_

#include <memory>

#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/android/synchronous_compositor.h"

namespace IPC {
class Message;
}

namespace blink {
class WebInputEvent;
}

namespace cc {
struct BeginFrameArgs;
}

namespace content {

class RenderWidgetHostViewAndroid;
class WebContents;
struct DidOverscrollParams;

class SynchronousCompositorBase : public SynchronousCompositor {
 public:
  static std::unique_ptr<SynchronousCompositorBase> Create(
      RenderWidgetHostViewAndroid* rwhva,
      WebContents* web_contents);

  ~SynchronousCompositorBase() override {}

  virtual void BeginFrame(const cc::BeginFrameArgs& args) = 0;
  virtual InputEventAckState HandleInputEvent(
      const blink::WebInputEvent& input_event) = 0;
  virtual void DidOverscroll(const DidOverscrollParams& over_scroll_params) = 0;
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

  virtual void DidBecomeCurrent() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_BASE_H_
