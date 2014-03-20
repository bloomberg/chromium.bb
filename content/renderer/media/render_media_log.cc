// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
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

  // Keep track of the latest buffered extents properties to avoid sending
  // thousands of events over IPC. See http://crbug.com/352585 for details.
  //
  // TODO(scherkus): We should overhaul MediaLog entirely to have clearer
  // separation of properties vs. events.
  if (event->type == media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED)
    last_buffered_extents_changed_event_.swap(event);
  else
    queued_media_events_.push_back(*event);

  // Limit the send rate of high frequency events.
  Time curr_time = Time::Now();
  if ((curr_time - last_ipc_send_time_) < TimeDelta::FromSeconds(1))
    return;
  last_ipc_send_time_ = curr_time;

  if (last_buffered_extents_changed_event_) {
    queued_media_events_.push_back(*last_buffered_extents_changed_event_);
    last_buffered_extents_changed_event_.reset();
  }

  DVLOG(1) << "media log events array size " << queued_media_events_.size();

  RenderThreadImpl::current()->Send(
      new ViewHostMsg_MediaLogEvents(queued_media_events_));
  queued_media_events_.clear();
}

RenderMediaLog::~RenderMediaLog() {}

}  // namespace content
