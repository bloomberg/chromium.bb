// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_log.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
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
      last_ipc_send_time_(tick_clock_->NowTicks()),
      ipc_send_pending_(false) {
  DCHECK(RenderThread::Get())
      << "RenderMediaLog must be constructed on the render thread";
}

void RenderMediaLog::AddEvent(scoped_ptr<media::MediaLogEvent> event) {
  Log(event.get());

  // For enforcing delay until it's been a second since the last ipc message was
  // sent.
  base::TimeDelta delay_for_next_ipc_send;

  {
    base::AutoLock auto_lock(lock_);

    // Keep track of the latest buffered extents properties to avoid sending
    // thousands of events over IPC. See http://crbug.com/352585 for details.
    if (event->type == media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED)
      last_buffered_extents_changed_event_.swap(event);
    else
      queued_media_events_.push_back(*event);

    if (ipc_send_pending_)
      return;

    ipc_send_pending_ = true;
    delay_for_next_ipc_send = base::TimeDelta::FromSeconds(1) -
                              (tick_clock_->NowTicks() - last_ipc_send_time_);
  }

  if (delay_for_next_ipc_send > base::TimeDelta()) {
    task_runner_->PostDelayedTask(
        FROM_HERE, base::Bind(&RenderMediaLog::SendQueuedMediaEvents, this),
        delay_for_next_ipc_send);
    return;
  }

  // It's been more than a second so send ASAP.
  if (task_runner_->BelongsToCurrentThread()) {
    SendQueuedMediaEvents();
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&RenderMediaLog::SendQueuedMediaEvents, this));
}

void RenderMediaLog::SendQueuedMediaEvents() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::vector<media::MediaLogEvent> events_to_send;
  {
    base::AutoLock auto_lock(lock_);

    DCHECK(ipc_send_pending_);
    ipc_send_pending_ = false;

    if (last_buffered_extents_changed_event_) {
      queued_media_events_.push_back(*last_buffered_extents_changed_event_);
      last_buffered_extents_changed_event_.reset();
    }

    queued_media_events_.swap(events_to_send);
    last_ipc_send_time_ = tick_clock_->NowTicks();
  }

  RenderThread::Get()->Send(new ViewHostMsg_MediaLogEvents(events_to_send));
}

RenderMediaLog::~RenderMediaLog() {
}

void RenderMediaLog::SetTickClockForTesting(
    scoped_ptr<base::TickClock> tick_clock) {
  base::AutoLock auto_lock(lock_);
  tick_clock_.swap(tick_clock);
  last_ipc_send_time_ = tick_clock_->NowTicks();
}

void RenderMediaLog::SetTaskRunnerForTesting(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
