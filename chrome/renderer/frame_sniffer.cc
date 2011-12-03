// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/frame_sniffer.h"

#include "base/logging.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"

FrameSniffer::FrameSniffer(content::RenderView* render_view,
                           const string16 &frame_name)
    : content::RenderViewObserver(render_view), frame_name_(frame_name) {
}

FrameSniffer::~FrameSniffer() {
}

void FrameSniffer::DidFailProvisionalLoad(WebKit::WebFrame* frame,
                                          const WebKit::WebURLError& error) {
  if (!ShouldSniffFrame(frame))
    return;
  Send(new ChromeViewHostMsg_FrameLoadingError(routing_id(), -error.reason));
}

void FrameSniffer::DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                            bool is_new_navigation) {
  if (!ShouldSniffFrame(frame))
    return;
  Send(new ChromeViewHostMsg_FrameLoadingCompleted(routing_id()));
}

bool FrameSniffer::ShouldSniffFrame(WebKit::WebFrame* frame) {
  return frame->name() == frame_name_;
}
