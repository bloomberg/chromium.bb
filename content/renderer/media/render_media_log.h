// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
#define CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_

#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "media/base/media_log.h"

namespace content {

// RenderMediaLog is an implementation of MediaLog that forwards events to the
// browser process, throttling as necessary.
//
// To minimize the number of events sent over the wire, only the latest event
// added is sent for high frequency events (e.g., BUFFERED_EXTENTS_CHANGED).
//
// It must be constructed on the render thread.
class CONTENT_EXPORT RenderMediaLog : public media::MediaLog {
 public:
  RenderMediaLog();

  // MediaLog implementation.
  void AddEvent(scoped_ptr<media::MediaLogEvent> event) override;

  // Will reset |last_ipc_send_time_| with the value of NowTicks().
  void SetTickClockForTesting(scoped_ptr<base::TickClock> tick_clock);
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

 private:
  ~RenderMediaLog() override;

  // Add event on the |task_runner_|.
  void AddEventInternal(scoped_ptr<media::MediaLogEvent> event);

  // Posted as a delayed task to throttle ipc message frequency.
  void SendQueuedMediaEvents();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<base::TickClock> tick_clock_;
  base::TimeTicks last_ipc_send_time_;
  std::vector<media::MediaLogEvent> queued_media_events_;

  // Limits the number buffered extents changed events we send over IPC to one.
  scoped_ptr<media::MediaLogEvent> last_buffered_extents_changed_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLog);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDER_MEDIA_LOG_H_
