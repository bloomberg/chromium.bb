// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/automation/automation_renderer_helper.h"

#include "base/basictypes.h"
#include "chrome/common/automation_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

using WebKit::WebFrame;
using WebKit::WebURL;

AutomationRendererHelper::AutomationRendererHelper(RenderView* render_view)
    : content::RenderViewObserver(render_view) {
}

AutomationRendererHelper::~AutomationRendererHelper() { }

void AutomationRendererHelper::WillPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  Send(new AutomationMsg_WillPerformClientRedirect(
      routing_id(), frame->identifier(), interval));
}

void AutomationRendererHelper::DidCancelClientRedirect(WebFrame* frame) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}

void AutomationRendererHelper::DidCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  Send(new AutomationMsg_DidCompleteOrCancelClientRedirect(
      routing_id(), frame->identifier()));
}
