// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_SCREEN_CAPTURE_DEVICE_CORE_H_
#define MEDIA_CAPTURE_CONTENT_SCREEN_CAPTURE_DEVICE_CORE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/capture/capture_export.h"
#include "media/capture/content/thread_safe_capture_oracle.h"
#include "media/capture/video/video_capture_device.h"

namespace base {
class Location;
}  // namespace base

namespace media {

struct VideoCaptureParams;

class ThreadSafeCaptureOracle;

// Keeps track of the video capture source frames and executes copying.
class CAPTURE_EXPORT VideoCaptureMachine {
 public:
  VideoCaptureMachine();
  virtual ~VideoCaptureMachine();

  // Starts capturing.
  // |callback| is invoked with true if succeeded. Otherwise, with false.
  virtual void Start(const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
                     const VideoCaptureParams& params,
                     const base::Callback<void(bool)> callback) = 0;

  // Suspend/Resume frame delivery. Implementations of these are optional.
  virtual void Suspend() {}
  virtual void Resume() {}

  // Stops capturing.
  // |callback| is invoked after the capturing has stopped.
  virtual void Stop(const base::Closure& callback) = 0;

  // Returns true if the video capture is configured to monitor end-to-end
  // system utilization, and alter frame sizes and/or frame rates to mitigate
  // overloading or under-utilization.
  virtual bool IsAutoThrottlingEnabled() const;

  // Called by ScreenCaptureDeviceCore when it failed to satisfy a "refresh
  // frame" request by attempting to resurrect the last video frame from the
  // buffer pool (this is referred to as the "passive" refresh approach).  The
  // failure can happen for a number of reasons (e.g., the oracle decided to
  // change resolution, or consumers of the last video frame are not yet
  // finished with it).
  //
  // The implementation of this method should consult the oracle, using the
  // kActiveRefreshRequest event type, to decide whether to initiate a new frame
  // capture, and then do so if the oracle agrees.
  virtual void MaybeCaptureForRefresh() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMachine);
};

// The "meat" of a content video capturer.
//
// Separating this from the "shell classes" WebContentsVideoCaptureDevice and
// DesktopCaptureDeviceAura allows safe destruction without needing to block any
// threads, as well as code sharing.
//
// ScreenCaptureDeviceCore manages a simple state machine and the pipeline
// (see notes at top of this file).  It times the start of successive captures
// and facilitates the processing of each through the stages of the
// pipeline.
class CAPTURE_EXPORT ScreenCaptureDeviceCore
    : public base::SupportsWeakPtr<ScreenCaptureDeviceCore> {
 public:
  ScreenCaptureDeviceCore(std::unique_ptr<VideoCaptureMachine> capture_machine);
  virtual ~ScreenCaptureDeviceCore();

  // Asynchronous requests to change ScreenCaptureDeviceCore state.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<VideoCaptureDevice::Client> client);
  void RequestRefreshFrame();
  void Suspend();
  void Resume();
  void StopAndDeAllocate();
  void OnConsumerReportingUtilization(int frame_feedback_id,
                                      double utilization);

 private:
  // Flag indicating current state.
  enum State { kIdle, kCapturing, kSuspended, kError, kLastCaptureState };

  void TransitionStateTo(State next_state);

  // Called back in response to StartCaptureMachine().  |success| is true if
  // capture machine succeeded to start.
  void CaptureStarted(bool success);

  // Stops capturing and notifies client_ of an error state.
  void Error(const base::Location& from_here, const std::string& reason);

  // Tracks that all activity occurs on the media stream manager's thread.
  base::ThreadChecker thread_checker_;

  // Current lifecycle state.
  State state_;

  // Tracks the CaptureMachine that's doing work on our behalf
  // on the device thread or UI thread.
  // This value should never be dereferenced by this class.
  std::unique_ptr<VideoCaptureMachine> capture_machine_;

  // Our thread-safe capture oracle which serves as the gateway to the video
  // capture pipeline. Besides the VideoCaptureDevice itself, it is the only
  // component of the system with direct access to |client_|.
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;

  // After Resume(), some unknown amount of time has passed, and the content of
  // the capture source may have changed. This flag is used to ensure that the
  // passive refresh mechanism is not used for the first refresh frame following
  // a Resume().
  bool force_active_refresh_once_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCaptureDeviceCore);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_SCREEN_CAPTURE_DEVICE_CORE_H_
