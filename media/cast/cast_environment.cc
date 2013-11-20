// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_environment.h"

#include "base/logging.h"

using base::TaskRunner;

namespace media {
namespace cast {

CastEnvironment::CastEnvironment(
    base::TickClock* clock,
    scoped_refptr<TaskRunner> main_thread_proxy,
    scoped_refptr<TaskRunner> audio_encode_thread_proxy,
    scoped_refptr<TaskRunner> audio_decode_thread_proxy,
    scoped_refptr<TaskRunner> video_encode_thread_proxy,
    scoped_refptr<TaskRunner> video_decode_thread_proxy,
    const CastLoggingConfig& config)
    : clock_(clock),
      main_thread_proxy_(main_thread_proxy),
      audio_encode_thread_proxy_(audio_encode_thread_proxy),
      audio_decode_thread_proxy_(audio_decode_thread_proxy),
      video_encode_thread_proxy_(video_encode_thread_proxy),
      video_decode_thread_proxy_(video_decode_thread_proxy),
      logging_(new LoggingImpl(clock, main_thread_proxy, config)) {
  DCHECK(main_thread_proxy) << "Main thread required";
}

CastEnvironment::~CastEnvironment() {}

bool CastEnvironment::PostTask(ThreadId identifier,
                          const tracked_objects::Location& from_here,
                          const base::Closure& task) {
  scoped_refptr<TaskRunner> task_runner =
      GetMessageTaskRunnerForThread(identifier);

  return task_runner->PostTask(from_here, task);
}

bool CastEnvironment::PostDelayedTask(ThreadId identifier,
                                 const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 base::TimeDelta delay) {
  scoped_refptr<TaskRunner> task_runner =
      GetMessageTaskRunnerForThread(identifier);

  return task_runner->PostDelayedTask(from_here, task, delay);
}

scoped_refptr<TaskRunner> CastEnvironment::GetMessageTaskRunnerForThread(
    ThreadId identifier) {
  switch (identifier) {
    case CastEnvironment::MAIN:
      return main_thread_proxy_;
    case CastEnvironment::AUDIO_ENCODER:
      return audio_encode_thread_proxy_;
    case CastEnvironment::AUDIO_DECODER:
      return audio_decode_thread_proxy_;
    case CastEnvironment::VIDEO_ENCODER:
      return video_encode_thread_proxy_;
    case CastEnvironment::VIDEO_DECODER:
      return video_decode_thread_proxy_;
    default:
      NOTREACHED() << "Invalid Thread identifier";
      return NULL;
  }
}

bool CastEnvironment::CurrentlyOn(ThreadId identifier) {
  switch (identifier) {
    case CastEnvironment::MAIN:
      return main_thread_proxy_->RunsTasksOnCurrentThread();
    case CastEnvironment::AUDIO_ENCODER:
      return audio_encode_thread_proxy_->RunsTasksOnCurrentThread();
    case CastEnvironment::AUDIO_DECODER:
      return audio_decode_thread_proxy_->RunsTasksOnCurrentThread();
    case CastEnvironment::VIDEO_ENCODER:
      return video_encode_thread_proxy_->RunsTasksOnCurrentThread();
    case CastEnvironment::VIDEO_DECODER:
      return video_decode_thread_proxy_->RunsTasksOnCurrentThread();
    default:
      NOTREACHED() << "Invalid thread identifier";
      return false;
  }
}

base::TickClock* CastEnvironment::Clock() const {
  return clock_;
}

LoggingImpl* CastEnvironment::Logging() {
  DCHECK(CurrentlyOn(CastEnvironment::MAIN)) <<
      "Must be called from main thread";
  return logging_.get();
}

}  // namespace cast
}  // namespace media
