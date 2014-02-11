// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_environment.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"

using base::SingleThreadTaskRunner;

namespace {

void DeleteLoggingOnMainThread(scoped_ptr<media::cast::LoggingImpl> logging) {
  logging.reset();
}

}  // namespace

namespace media {
namespace cast {

CastEnvironment::CastEnvironment(
    scoped_ptr<base::TickClock> clock,
    scoped_refptr<SingleThreadTaskRunner> main_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> audio_encode_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> audio_decode_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> video_encode_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> video_decode_thread_proxy,
    scoped_refptr<SingleThreadTaskRunner> transport_thread_proxy,
    const CastLoggingConfig& config)
    : clock_(clock.Pass()),
      main_thread_proxy_(main_thread_proxy),
      audio_encode_thread_proxy_(audio_encode_thread_proxy),
      audio_decode_thread_proxy_(audio_decode_thread_proxy),
      video_encode_thread_proxy_(video_encode_thread_proxy),
      video_decode_thread_proxy_(video_decode_thread_proxy),
      transport_thread_proxy_(transport_thread_proxy),
      logging_(new LoggingImpl(main_thread_proxy, config)) {
  DCHECK(main_thread_proxy);
}

CastEnvironment::~CastEnvironment() {
  // Logging must be deleted on the main thread.
  if (main_thread_proxy_->RunsTasksOnCurrentThread()) {
    logging_.reset();
  } else {
    main_thread_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&DeleteLoggingOnMainThread, base::Passed(&logging_)));
  }
}

bool CastEnvironment::PostTask(ThreadId identifier,
                               const tracked_objects::Location& from_here,
                               const base::Closure& task) {
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      GetMessageSingleThreadTaskRunnerForThread(identifier);

  return task_runner->PostTask(from_here, task);
}

bool CastEnvironment::PostDelayedTask(
    ThreadId identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  scoped_refptr<SingleThreadTaskRunner> task_runner =
      GetMessageSingleThreadTaskRunnerForThread(identifier);

  return task_runner->PostDelayedTask(from_here, task, delay);
}

scoped_refptr<SingleThreadTaskRunner>
CastEnvironment::GetMessageSingleThreadTaskRunnerForThread(
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
    case CastEnvironment::TRANSPORT:
      return transport_thread_proxy_;
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
    case CastEnvironment::TRANSPORT:
      return transport_thread_proxy_->RunsTasksOnCurrentThread();
    default:
      NOTREACHED() << "Invalid thread identifier";
      return false;
  }
}

base::TickClock* CastEnvironment::Clock() const { return clock_.get(); }

LoggingImpl* CastEnvironment::Logging() {
  DCHECK(CurrentlyOn(CastEnvironment::MAIN))
      << "Must be called from main thread";
  return logging_.get();
}

}  // namespace cast
}  // namespace media
