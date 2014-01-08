// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_CAST_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_impl.h"

namespace media {
namespace cast {

class CastEnvironment : public base::RefCountedThreadSafe<CastEnvironment> {
 public:
  // An enumeration of the cast threads.
  enum ThreadId {
    // The main thread is where the cast system is configured and where timers
    // and network IO is performed.
    MAIN,
    // The audio encoder thread is where all send side audio processing is done,
    // primarily encoding but also re-sampling.
    AUDIO_ENCODER,
    // The audio decoder thread is where all receive side audio processing is
    // done, primarily decoding but also error concealment and re-sampling.
    AUDIO_DECODER,
    // The video encoder thread is where the video encode processing is done.
    VIDEO_ENCODER,
    // The video decoder thread is where the video decode processing is done.
    VIDEO_DECODER,
  };

  CastEnvironment(base::TickClock* clock,
                  scoped_refptr<base::TaskRunner> main_thread_proxy,
                  scoped_refptr<base::TaskRunner> audio_encode_thread_proxy,
                  scoped_refptr<base::TaskRunner> audio_decode_thread_proxy,
                  scoped_refptr<base::TaskRunner> video_encode_thread_proxy,
                  scoped_refptr<base::TaskRunner> video_decode_thread_proxy,
                  const CastLoggingConfig& config);

  // These are the same methods in message_loop.h, but are guaranteed to either
  // get posted to the MessageLoop if it's still alive, or be deleted otherwise.
  // They return true iff the thread existed and the task was posted.  Note that
  // even if the task is posted, there's no guarantee that it will run, since
  // the target thread may already have a Quit message in its queue.
  bool PostTask(ThreadId identifier,
                const tracked_objects::Location& from_here,
                const base::Closure& task);

  bool PostDelayedTask(ThreadId identifier,
                       const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay);

  bool CurrentlyOn(ThreadId identifier);

  base::TickClock* Clock() const;

  // Logging is not thread safe. Should always be called from the main thread.
  LoggingImpl* Logging();

 protected:
  virtual ~CastEnvironment();

 private:
  friend class base::RefCountedThreadSafe<CastEnvironment>;

  scoped_refptr<base::TaskRunner> GetMessageTaskRunnerForThread(
      ThreadId identifier);

  base::TickClock* const clock_;  // Not owned by this class.
  scoped_refptr<base::TaskRunner> main_thread_proxy_;
  scoped_refptr<base::TaskRunner> audio_encode_thread_proxy_;
  scoped_refptr<base::TaskRunner> audio_decode_thread_proxy_;
  scoped_refptr<base::TaskRunner> video_encode_thread_proxy_;
  scoped_refptr<base::TaskRunner> video_decode_thread_proxy_;

  scoped_ptr<LoggingImpl> logging_;

  DISALLOW_COPY_AND_ASSIGN(CastEnvironment);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_ENVIRONMENT_H_
