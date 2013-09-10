// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_THREAD_H_
#define MEDIA_CAST_CAST_THREAD_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"

namespace media {
namespace cast {

using base::MessageLoopProxy;

class CastThread : public base::RefCountedThreadSafe<CastThread> {
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

  CastThread(scoped_refptr<MessageLoopProxy> main_thread_proxy,
             scoped_refptr<MessageLoopProxy> audio_encode_thread_proxy,
             scoped_refptr<MessageLoopProxy> audio_decode_thread_proxy,
             scoped_refptr<MessageLoopProxy> video_encode_thread_proxy,
             scoped_refptr<MessageLoopProxy> video_decode_thread_proxy);

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

 private:
  scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxyForThread(
      ThreadId identifier);

  scoped_refptr<MessageLoopProxy> main_thread_proxy_;
  scoped_refptr<MessageLoopProxy> audio_encode_thread_proxy_;
  scoped_refptr<MessageLoopProxy> audio_decode_thread_proxy_;
  scoped_refptr<MessageLoopProxy> video_encode_thread_proxy_;
  scoped_refptr<MessageLoopProxy> video_decode_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CastThread);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_THREAD_H_
