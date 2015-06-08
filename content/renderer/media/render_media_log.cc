// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_thread.h"

namespace {

// Print an event to the chromium log.
void Log(media::MediaLogEvent* event) {
  if (event->type == media::MediaLogEvent::PIPELINE_ERROR) {
    LOG(ERROR) << "MediaEvent: "
               << media::MediaLog::MediaEventToLogString(*event);
  } else if (event->type != media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED &&
             event->type != media::MediaLogEvent::PROPERTY_CHANGE &&
             event->type != media::MediaLogEvent::NETWORK_ACTIVITY_SET) {
    DVLOG(1) << "MediaEvent: "
             << media::MediaLog::MediaEventToLogString(*event);
  }
}

}  // namespace

namespace content {

RenderMediaLog::RenderMediaLog()
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      tick_clock_(new base::DefaultTickClock()),
      last_ipc_send_time_(tick_clock_->NowTicks()) {
  DCHECK(RenderThread::Get())
      << "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(scoped_ptr<media::MediaLogEvent> event) {
  // Always post to preserve the correct order of events.
  // TODO(xhwang): Consider using sorted containers to keep the order and
  // avoid extra posting.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&RenderMediaLog::AddEventInternal, this,
                                    base::Passed(&event)));
}

void RenderMediaLog::AddEventInternal(scoped_ptr<media::MediaLogEvent> event) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  Log(event.get());

  // If there is an event waiting to be sent, there must be a send task pending.
  const bool delayed_ipc_send_pending =
      !queued_media_events_.empty() || last_buffered_extents_changed_event_;

  // Keep track of the latest buffered extents properties to avoid sending
  // thousands of events over IPC. See http://crbug.com/352585 for details.
  if (event->type == media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED)
    last_buffered_extents_changed_event_.swap(event);
  else
    queued_media_events_.push_back(*event);

  if (delayed_ipc_send_pending)
    return;

  // Delay until it's been a second since the last ipc message was sent.
  base::TimeDelta delay_for_next_ipc_send =
      base::TimeDelta::FromSeconds(1) -
      (tick_clock_->NowTicks() - last_ipc_send_time_);
  if (delay_for_next_ipc_send > base::TimeDelta()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&RenderMediaLog::SendQueuedMediaEvents, this),
        delay_for_next_ipc_send);
    return;
  }

  // It's been more than a second so send now.
  SendQueuedMediaEvents();
}

void RenderMediaLog::SendQueuedMediaEvents() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (last_buffered_extents_changed_event_) {
    queued_media_events_.push_back(*last_buffered_extents_changed_event_);
    last_buffered_extents_changed_event_.reset();
  }

  RenderThread::Get()->Send(
      new ViewHostMsg_MediaLogEvents(queued_media_events_));
  queued_media_events_.clear();
  last_ipc_send_time_ = tick_clock_->NowTicks();
}

RenderMediaLog::~RenderMediaLog() {
}

void RenderMediaLog::SetTickClockForTesting(
    scoped_ptr<base::TickClock> tick_clock) {
  tick_clock_.swap(tick_clock);
  last_ipc_send_time_ = tick_clock_->NowTicks();
}

void RenderMediaLog::SetTaskRunnerForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
