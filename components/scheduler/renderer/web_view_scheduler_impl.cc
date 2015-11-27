// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/logging.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"

namespace scheduler {

WebViewSchedulerImpl::WebViewSchedulerImpl(
    blink::WebView* web_view,
    RendererSchedulerImpl* renderer_scheduler)
    : web_view_(web_view),
      renderer_scheduler_(renderer_scheduler),
      background_(false) {}

WebViewSchedulerImpl::~WebViewSchedulerImpl() {
  // TODO(alexclarke): Find out why we can't rely on the web view outliving the
  // frame.
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->DetachFromWebViewScheduler();
  }
}

void WebViewSchedulerImpl::setPageInBackground(bool background) {
  background_ = background;
  // TODO(alexclarke): Do something with this flag.
}

blink::WebPassOwnPtr<blink::WebFrameScheduler>
WebViewSchedulerImpl::createFrameScheduler() {
  scoped_ptr<WebFrameSchedulerImpl> frame_scheduler(
      new WebFrameSchedulerImpl(renderer_scheduler_, this));
  frame_schedulers_.insert(frame_scheduler.get());
  return blink::adoptWebPtr(frame_scheduler.release());
}

void WebViewSchedulerImpl::Unregister(WebFrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

}  // namespace scheduler
