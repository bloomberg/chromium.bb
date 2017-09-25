// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/web_contents_event_forwarder.h"

#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebMouseEvent.h"

using blink::WebGestureEvent;
using blink::WebMouseEvent;
using blink::WebInputEvent;

namespace vr {

WebContentsEventForwarder::WebContentsEventForwarder(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebContentsEventForwarder::~WebContentsEventForwarder() = default;

void WebContentsEventForwarder::ForwardEvent(
    std::unique_ptr<WebInputEvent> event) {
  if (!web_contents_->GetRenderWidgetHostView()) {
    return;
  }
  content::RenderWidgetHost* rwh =
      web_contents_->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (!rwh) {
    return;
  }

  if (WebInputEvent::IsMouseEventType(event->GetType())) {
    rwh->ForwardMouseEvent(static_cast<const WebMouseEvent&>(*event));
  } else if (WebInputEvent::IsGestureEventType(event->GetType())) {
    rwh->ForwardGestureEvent(static_cast<const WebGestureEvent&>(*event));
  }
}

}  // namespace vr
