// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_COMPONENT_DEVICE_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_COMPONENT_DEVICE_H_

#include <stdint.h>
#include <string>

#include "cast_key_system.h"

namespace chromecast {
namespace media {
class CastDecoderBuffer;
class DecryptContext;

// Common base interface for both platform-specific audio and video pipeline
// backends.  Both follow this state machine:
//                       +------------- kRunning <--+
//                       |                 ^        |
//                       v                 |        |
// kUninitialized <--> kIdle  -------------+        |
//                       ^                 |        |
//                       |                 v        |
//                       +------------- kPaused <---+
// {any state} --> kError
// kError --> kUninitialized
//
// Notes:
// - Hardware resources should be acquired when transitioning from the
//   |kUninitialized| state to the |kIdle| state.
// - Buffers will be pushed only in the kRunning or kPaused states.
// - The end of stream is signaled through a special buffer.
//   Once the end of stream buffer is fed, no other buffer
//   will be fed until the FSM goes through the kIdle state again.
// - In both kPaused and kRunning states, frames can be fed.
//   However, frames are possibly rendered only in the kRunning state.
// - In the kRunning state, frames are rendered according to the clock rate.
// - All the hardware resources must be released in the |kError| state.
class MediaComponentDevice {
 public:
  enum State {
    kStateUninitialized,
    kStateIdle,
    kStateRunning,
    kStatePaused,
    kStateError,
  };

  enum FrameStatus {
    kFrameSuccess,
    kFrameFailed,
    kFramePending,
  };

  // Interface for receiving status when PushFrame has completed.
  class FrameStatusCB {
   public:
    virtual ~FrameStatusCB() {}
    virtual void Run(FrameStatus status) = 0;
  };

  // Client callbacks interface
  class Client {
   public:
    virtual ~Client() {}
    virtual void OnEndOfStream() = 0;
  };

  // The statistics are computed since the media component left the idle state.
  // For video, a sample is defined as a frame.
  struct Statistics {
    uint64_t decoded_bytes;
    uint64_t decoded_samples;
    uint64_t dropped_samples;
  };

  // Info on pipeline latency: amount of data in pipeline not rendered yet,
  // and timestamp of system clock (must be CLOCK_MONOTONIC) at which delay
  // measurement was taken.  Both times in microseconds.
  struct RenderingDelay {
    RenderingDelay()
        : delay_microseconds(INT64_MIN), timestamp_microseconds(INT64_MIN) {}
    RenderingDelay(int64_t delay_microseconds_in,
                   int64_t timestamp_microseconds_in)
        : delay_microseconds(delay_microseconds_in),
          timestamp_microseconds(timestamp_microseconds_in) {}
    int64_t delay_microseconds;
    int64_t timestamp_microseconds;
  };

  virtual ~MediaComponentDevice() {}

  // Registers |client| as the media event handler.  Implementation
  // takes ownership of |client| and call OnEndOfStream when an end-of-stream
  // buffer is processed.
  virtual void SetClient(Client* client) = 0;

  // Changes the state and performs any necessary transitions.
  // Returns true when successful.
  virtual bool SetState(State new_state) = 0;

  // Returns the current state of the media component.
  virtual State GetState() const = 0;

  // Sets the time where rendering should start.
  // Return true when successful.
  // Will only be invoked in state kStateIdle.
  virtual bool SetStartPts(int64_t microseconds) = 0;

  // Pushes a frame.  If the implementation cannot push the buffer
  // now, it must store the buffer, return |kFramePending| and execute the push
  // at a later time when it becomes possible to do so.  The implementation must
  // then invoke |completion_cb|.  Pushing a pending frame should be aborted if
  // the state returns to kStateIdle, and |completion_cb| need not be invoked.
  // If |kFramePending| is returned, the pipeline will stop pushing any further
  // buffers until the |completion_cb| is invoked.
  // Ownership: |decrypt_context|, |buffer|, |completion_cb| are all owned by
  // the implementation and must be deleted once no longer needed (even in the
  // case where |completion_cb| is not called).
  // |completion_cb| should be only be invoked to indicate completion of a
  // pending buffer push - not for the immediate |kFrameSuccess| return case.
  virtual FrameStatus PushFrame(DecryptContext* decrypt_context,
                                CastDecoderBuffer* buffer,
                                FrameStatusCB* completion_cb) = 0;

  // Returns the pipeline latency: i.e. the amount of data
  // in the pipeline that have not been rendered yet, in microseconds.
  // Returns delay = INT64_MIN if the latency is not available.
  virtual RenderingDelay GetRenderingDelay() const = 0;

  // Returns the playback statistics since the last transition from idle state.
  // Returns true when successful.
  // Is only invoked in state kStateRunning.
  virtual bool GetStatistics(Statistics* stats) const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_H_
