// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread_impl.h"

using base::Time;
using base::TimeDelta;

namespace content {

RenderMediaLog::RenderMediaLog()
    : render_loop_(base::MessageLoopProxy::current()),
      last_ipc_send_time_(Time::Now()) {
  DCHECK(RenderThreadImpl::current()) <<
      "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(scoped_ptr<media::MediaLogEvent> event) {
  if (!RenderThreadImpl::current()) {
    render_loop_->PostTask(FROM_HERE, base::Bind(
        &RenderMediaLog::AddEvent, this, base::Passed(&event)));
    return;
  }
  queued_media_events_.push_back(*event);
  // Limit the send rate of high frequency events.
  Time curr_time = Time::Now();
  if ((curr_time - last_ipc_send_time_) < TimeDelta::FromSeconds(1))
    return;
  last_ipc_send_time_ = curr_time;
  DVLOG(1) << "media log events array size " << queued_media_events_.size();
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_MediaLogEvents(queued_media_events_));
  queued_media_events_.clear();
}

RenderMediaLog::~RenderMediaLog() {}

}  // namespace content
