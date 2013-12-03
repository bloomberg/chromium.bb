// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_DEVICE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_DEVICE_IMPL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "content/browser/renderer_host/media/video_capture_oracle.h"
#include "content/common/content_export.h"
#include "media/video/capture/video_capture_device.h"

namespace content {

const int kMinFrameWidth = 2;
const int kMinFrameHeight = 2;

// Returns the nearest even integer closer to zero.
template<typename IntType>
IntType MakeEven(IntType x) {
  return x & static_cast<IntType>(-2);
}

// TODO(nick): Remove this once frame subscription is supported on Aura and
// Linux.
#if (defined(OS_WIN) || defined(OS_MACOSX)) || defined(USE_AURA)
const bool kAcceleratedSubscriberIsSupported = true;
#else
const bool kAcceleratedSubscriberIsSupported = false;
#endif

class VideoCaptureMachine;

// Thread-safe, refcounted proxy to the VideoCaptureOracle.  This proxy wraps
// the VideoCaptureOracle, which decides which frames to capture, and a
// VideoCaptureDevice::Client, which allocates and receives the captured
// frames, in a lock to synchronize state between the two.
class ThreadSafeCaptureOracle
    : public base::RefCountedThreadSafe<ThreadSafeCaptureOracle> {
 public:
  ThreadSafeCaptureOracle(scoped_ptr<media::VideoCaptureDevice::Client> client,
                          scoped_ptr<VideoCaptureOracle> oracle,
                          const media::VideoCaptureParams& params);

  // Called when a captured frame is available or an error has occurred.
  // If |success| is true then the frame provided is valid and |timestamp|
  // indicates when the frame was painted.
  // If |success| is false, both the frame provided and |timestamp| are invalid.
  typedef base::Callback<void(base::Time timestamp, bool success)>
      CaptureFrameCallback;

  bool ObserveEventAndDecideCapture(VideoCaptureOracle::Event event,
                                    base::Time event_time,
                                    scoped_refptr<media::VideoFrame>* storage,
                                    CaptureFrameCallback* callback);

  base::TimeDelta capture_period() const {
    return oracle_->capture_period();
  }

  // Updates capture resolution based on the supplied source size and the
  // maximum frame size.
  void UpdateCaptureSize(const gfx::Size& source_size);

  // Stop new captures from happening (but doesn't forget the client).
  void Stop();

  // Signal an error to the client.
  void ReportError();

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeCaptureOracle>;
  virtual ~ThreadSafeCaptureOracle();

  // Callback invoked on completion of all captures.
  void DidCaptureFrame(
      scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      int frame_number,
      base::Time timestamp,
      bool success);
  // Protects everything below it.
  base::Lock lock_;

  // Recipient of our capture activity.
  scoped_ptr<media::VideoCaptureDevice::Client> client_;

  // Makes the decision to capture a frame.
  const scoped_ptr<VideoCaptureOracle> oracle_;

  // The video capture parameters used to construct the oracle proxy.
  const media::VideoCaptureParams params_;

  // Indicates if capture size has been updated after construction.
  bool capture_size_updated_;

  // The current capturing resolution and frame rate.
  gfx::Size capture_size_;
  int frame_rate_;
};

// Keeps track of the video capture source frames and executes copying on the
// UI BrowserThread.
class VideoCaptureMachine {
 public:
  VideoCaptureMachine() : started_(false) {}
  virtual ~VideoCaptureMachine() {}

  // This should only be checked on the UI thread.
  bool started() const { return started_; }

  // Starts capturing. Returns true if succeeded.
  // Must be run on the UI BrowserThread.
  virtual bool Start(
      const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy) = 0;

  // Stops capturing. Must be run on the UI BrowserThread.
  virtual void Stop() = 0;

 protected:
  bool started_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMachine);
};

// The "meat" of the video capture implementation.
//
// Separating this from the "shell classes" WebContentsVideoCaptureDevice and
// BrowserCompositorCaptureDevice allows safe destruction without needing to
// block any threads (e.g., the IO BrowserThread), as well as code sharing.
//
// VideoCaptureDeviceImpl manages a simple state machine and the pipeline (see
// notes at top of this file).  It times the start of successive
// captures and facilitates the processing of each through the stages of the
// pipeline.
class CONTENT_EXPORT VideoCaptureDeviceImpl
    : public base::SupportsWeakPtr<VideoCaptureDeviceImpl> {
 public:
  VideoCaptureDeviceImpl(scoped_ptr<VideoCaptureMachine> capture_machine);
  virtual ~VideoCaptureDeviceImpl();

  // Asynchronous requests to change VideoCaptureDeviceImpl state.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        scoped_ptr<media::VideoCaptureDevice::Client> client);
  void StopAndDeAllocate();

 private:
  // Flag indicating current state.
  enum State {
    kIdle,
    kCapturing,
    kError
  };

  void TransitionStateTo(State next_state);

  // Called on the IO thread in response to StartCaptureMachine().
  // |success| is true if capture machine succeeded to start.
  void CaptureStarted(bool success);

  // Stops capturing and notifies client_ of an error state.
  void Error();

  // Tracks that all activity occurs on the media stream manager's thread.
  base::ThreadChecker thread_checker_;

  // Current lifecycle state.
  State state_;

  // Tracks the CaptureMachine that's doing work on our behalf on the UI thread.
  // This value should never be dereferenced by this class, other than to
  // create and destroy it on the UI thread.
  scoped_ptr<VideoCaptureMachine> capture_machine_;

  // Our thread-safe capture oracle which serves as the gateway to the video
  // capture pipeline. Besides the WCVCD itself, it is the only component of the
  // system with direct access to |client_|.
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceImpl);
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_VIDEO_CAPTURE_DEVICE_IMPL_H_
