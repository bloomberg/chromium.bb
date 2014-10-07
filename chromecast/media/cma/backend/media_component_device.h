// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;
class DecryptContext;

// MediaComponentDevice -
//
// State machine:
//                       -------------- kRunning <---
//                       |                 ^        |
//                       v                 |        |
// kUninitialized <--> kIdle  --------------        |
//                       ^                 |        |
//                       |                 v        |
//                       -------------- kPaused <----
// {any state} --> kError
// kError --> kUninitialized
//
// Notes:
// - Hardware resources are acquired when transitioning from the
//   |kUninitialized| state to the |kIdle| state.
// - Buffers can be pushed only in the kRunning or kPaused states.
// - The end of stream is signaled through a special buffer.
//   Once the end of stream buffer is fed, no other buffer
//   can be fed until the FSM goes through the kIdle state again.
// - In both kPaused and kRunning states, frames can be fed.
//   However, frames are possibly rendered only in the kRunning state.
// - In the kRunning state, frames are rendered according to the clock rate.
// - All the hardware resources must be released in the |kError| state.
//
class MediaComponentDevice
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
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
  typedef base::Callback<void(FrameStatus)> FrameStatusCB;

  struct Client {
    Client();
    ~Client();

    // Invoked when playback reaches the end of stream.
    base::Closure eos_cb;
  };

  // The statistics are computed since the media component left the idle state.
  // For video, a sample is defined as a frame.
  struct Statistics {
    uint64 decoded_bytes;
    uint64 decoded_samples;
    uint64 dropped_samples;
  };

  // Returns whether or not transitioning from |state1| to |state2| is valid.
  static bool IsValidStateTransition(State state1, State state2);

  // Returns string representation of state (for logging)
  static std::string StateToString(const State& state);

  MediaComponentDevice();
  virtual ~MediaComponentDevice();

  // Register |client| as the media event handler.
  virtual void SetClient(const Client& client) = 0;

  // Changes the state and performs any necessary transitions.
  // Returns true when successful.
  virtual bool SetState(State new_state) = 0;

  // Returns the current state of the media component.
  virtual State GetState() const = 0;

  // Sets the time where rendering should start.
  // Return true when successful.
  // Can only be invoked in state kStateIdle.
  virtual bool SetStartPts(base::TimeDelta time) = 0;

  // Pushes a frame.
  // |completion_cb| is only invoked if the returned value is |kFramePending|.
  // In this specific case, no additional frame can be pushed before
  // |completion_cb| is invoked.
  // Note: |completion_cb| cannot be invoked with |kFramePending|.
  // Note: pushing the pending frame should be aborted when the state goes back
  // to kStateIdle. |completion_cb| is not invoked in that case.
  virtual FrameStatus PushFrame(
      const scoped_refptr<DecryptContext>& decrypt_context,
      const scoped_refptr<DecoderBufferBase>& buffer,
      const FrameStatusCB& completion_cb) = 0;

  // Returns the rendering time of the latest rendered sample.
  // Can be invoked only in states kStateRunning or kStatePaused.
  // Returns |kNoTimestamp()| if the playback time cannot be retrieved.
  virtual base::TimeDelta GetRenderingTime() const = 0;

  // Returns the pipeline latency: i.e. the amount of data
  // in the pipeline that have not been rendered yet.
  // Returns |kNoTimestamp()| if the latency is not available.
  virtual base::TimeDelta GetRenderingDelay() const = 0;

  // Returns the playback statistics. Statistics are computed since the media
  // component left the idle state.
  // Returns true when successful.
  // Can only be invoked in state kStateRunning.
  virtual bool GetStatistics(Statistics* stats) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaComponentDevice);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_COMPONENT_DEVICE_H_
