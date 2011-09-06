// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/frame_sniffer.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/navigation_state.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"

FrameSniffer::FrameSniffer(RenderView* render_view, const string16 &frame_name)
    : RenderViewObserver(render_view), frame_name_(frame_name) {
}

FrameSniffer::~FrameSniffer() {
}

void FrameSniffer::DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                    const WebKit::WebURLError& error) {
  if (!ShouldSniffFrame(frame))
    return;
  Send(new ChromeViewHostMsg_FrameLoadingError(routing_id(), -error.reason));
}

bool FrameSniffer::ShouldSniffFrame(WebKit::WebFrame* frame) {
  return frame->name() == frame_name_;
}
