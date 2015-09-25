// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/web_frame_host_scheduler_impl.h"

#include "base/logging.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"

namespace scheduler {

WebFrameHostSchedulerImpl::WebFrameHostSchedulerImpl(
    RendererScheduler* render_scheduler)
    : render_scheduler_(render_scheduler), background_(false) {}

WebFrameHostSchedulerImpl::~WebFrameHostSchedulerImpl() {
  DCHECK(frame_schedulers_.empty()) << "WebFrameHostSchedulerImpl should "
                                       "outlive its WebFrameSchedulers";
}

void WebFrameHostSchedulerImpl::setPageInBackground(bool background) {
  background_ = background;
  // TODO(alexclarke): Do something with this flag.
}

blink::WebFrameScheduler* WebFrameHostSchedulerImpl::createFrameScheduler() {
  WebFrameSchedulerImpl* frame_scheduler =
      new WebFrameSchedulerImpl(render_scheduler_, this);
  frame_schedulers_.insert(frame_scheduler);
  return frame_scheduler;
}

void WebFrameHostSchedulerImpl::Unregister(
    WebFrameSchedulerImpl* frame_scheduler) {
  DCHECK(frame_schedulers_.find(frame_scheduler) != frame_schedulers_.end());
  frame_schedulers_.erase(frame_scheduler);
}

}  // namespace scheduler
