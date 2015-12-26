// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_view_scheduler_impl.h"

#include "base/logging.h"
#include "components/scheduler/base/virtual_time_domain.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "third_party/WebKit/public/platform/WebFrameScheduler.h"

namespace scheduler {

WebViewSchedulerImpl::WebViewSchedulerImpl(
    blink::WebView* web_view,
    RendererSchedulerImpl* renderer_scheduler,
    bool disable_background_timer_throttling)
    : web_view_(web_view),
      renderer_scheduler_(renderer_scheduler),
      page_in_background_(false),
      disable_background_timer_throttling_(
          disable_background_timer_throttling) {}

WebViewSchedulerImpl::~WebViewSchedulerImpl() {
  // TODO(alexclarke): Find out why we can't rely on the web view outliving the
  // frame.
  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->DetachFromWebViewScheduler();
  }
}

void WebViewSchedulerImpl::setPageInBackground(bool page_in_background) {
  if (disable_background_timer_throttling_ ||
      page_in_background_ == page_in_background)
    return;

  page_in_background_ = page_in_background;

  for (WebFrameSchedulerImpl* frame_scheduler : frame_schedulers_) {
    frame_scheduler->SetPageInBackground(page_in_background_);
  }
}

scoped_ptr<WebFrameSchedulerImpl>
WebViewSchedulerImpl::createWebFrameSchedulerImpl() {
  scoped_ptr<WebFrameSchedulerImpl> frame_scheduler(
      new WebFrameSchedulerImpl(renderer_scheduler_, this));
  frame_scheduler->SetPageInBackground(page_in_background_);
  frame_schedulers_.insert(frame_scheduler.get());
  return frame_scheduler;
}

blink::WebPassOwnPtr<blink::WebFrameScheduler>
WebViewSchedulerImpl::createFrameScheduler() {
  return blink::adoptWebPtr(createWebFrameSchedulerImpl().release());
}

void WebViewSchedulerImpl::Unregister(WebFrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

}  // namespace scheduler
