// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_device_impl.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "ui/gfx/rect.h"

namespace content {

namespace {

void DeleteCaptureMachineOnUIThread(
    scoped_ptr<VideoCaptureMachine> capture_machine) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (capture_machine) {
    capture_machine->Stop();
    capture_machine.reset();
  }
}

}  // namespace

ThreadSafeCaptureOracle::ThreadSafeCaptureOracle(
    scoped_ptr<media::VideoCaptureDevice::Client> client,
    scoped_ptr<VideoCaptureOracle> oracle,
    const media::VideoCaptureParams& params)
    : client_(client.Pass()),
      oracle_(oracle.Pass()),
      params_(params),
      capture_size_updated_(false) {
  // Frame dimensions must each be an even integer since the client wants (or
  // will convert to) YUV420.
  capture_size_ = gfx::Size(
      MakeEven(params.requested_format.frame_size.width()),
      MakeEven(params.requested_format.frame_size.height()));
  frame_rate_ = params.requested_format.frame_rate;
}

ThreadSafeCaptureOracle::~ThreadSafeCaptureOracle() {}

bool ThreadSafeCaptureOracle::ObserveEventAndDecideCapture(
    VideoCaptureOracle::Event event,
    base::Time event_time,
    scoped_refptr<media::VideoFrame>* storage,
    CaptureFrameCallback* callback) {
  base::AutoLock guard(lock_);

  if (!client_)
    return false;  // Capture is stopped.

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer =
      client_->ReserveOutputBuffer(media::VideoFrame::I420, capture_size_);
  const bool should_capture =
      oracle_->ObserveEventAndDecideCapture(event, event_time);
  const bool content_is_dirty =
      (event == VideoCaptureOracle::kCompositorUpdate ||
       event == VideoCaptureOracle::kSoftwarePaint);
  const char* event_name =
      (event == VideoCaptureOracle::kTimerPoll ? "poll" :
       (event == VideoCaptureOracle::kCompositorUpdate ? "gpu" :
       "paint"));

  // Consider the various reasons not to initiate a capture.
  if (should_capture && !output_buffer) {
    TRACE_EVENT_INSTANT1("mirroring",
                         "EncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger",
                         event_name);
    return false;
  } else if (!should_capture && output_buffer) {
    if (content_is_dirty) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("mirroring", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD,
                           "trigger", event_name);
    }
    return false;
  } else if (!should_capture && !output_buffer) {
    // We decided not to capture, but we wouldn't have been able to if we wanted
    // to because no output buffer was available.
    TRACE_EVENT_INSTANT1("mirroring", "NearlyEncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger", event_name);
    return false;
  }
  int frame_number = oracle_->RecordCapture();
  TRACE_EVENT_ASYNC_BEGIN2("mirroring", "Capture", output_buffer.get(),
                           "frame_number", frame_number,
                           "trigger", event_name);
  *callback = base::Bind(&ThreadSafeCaptureOracle::DidCaptureFrame,
                         this,
                         output_buffer,
                         frame_number);
  *storage = media::VideoFrame::WrapExternalPackedMemory(
      media::VideoFrame::I420,
      capture_size_,
      gfx::Rect(capture_size_),
      capture_size_,
      static_cast<uint8*>(output_buffer->data()),
      output_buffer->size(),
      base::SharedMemory::NULLHandle(),
      base::TimeDelta(),
      base::Closure());
  return true;
}

void ThreadSafeCaptureOracle::UpdateCaptureSize(const gfx::Size& source_size) {
  base::AutoLock guard(lock_);

  // If this is the first call to UpdateCaptureSize(), or the receiver supports
  // variable resolution, then determine the capture size by treating the
  // requested width and height as maxima.
  if (!capture_size_updated_ || params_.allow_resolution_change) {
    // The capture resolution should not exceed the source frame size.
    // In other words it should downscale the image but not upscale it.
    if (source_size.width() > params_.requested_format.frame_size.width() ||
        source_size.height() > params_.requested_format.frame_size.height()) {
      gfx::Rect capture_rect = media::ComputeLetterboxRegion(
          gfx::Rect(params_.requested_format.frame_size), source_size);
      capture_size_ = gfx::Size(MakeEven(capture_rect.width()),
                                MakeEven(capture_rect.height()));
    } else {
      capture_size_ = gfx::Size(MakeEven(source_size.width()),
                                MakeEven(source_size.height()));
    }
    capture_size_updated_ = true;
  }
}

void ThreadSafeCaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  client_.reset();
}

void ThreadSafeCaptureOracle::ReportError() {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnError();
}

void ThreadSafeCaptureOracle::DidCaptureFrame(
    scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer,
    int frame_number,
    base::Time timestamp,
    bool success) {
  base::AutoLock guard(lock_);
  TRACE_EVENT_ASYNC_END2("mirroring", "Capture", buffer.get(),
                         "success", success,
                         "timestamp", timestamp.ToInternalValue());

  if (!client_)
    return;  // Capture is stopped.

  if (success) {
    if (oracle_->CompleteCapture(frame_number, timestamp)) {
      client_->OnIncomingCapturedBuffer(buffer,
                                        media::VideoFrame::I420,
                                        capture_size_,
                                        timestamp,
                                        frame_rate_);
    }
  }
}

void VideoCaptureDeviceImpl::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<media::VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (params.requested_format.frame_rate <= 0) {
    DVLOG(1) << "invalid frame_rate: " << params.requested_format.frame_rate;
    client->OnError();
    return;
  }

  if (params.requested_format.frame_size.width() < kMinFrameWidth ||
      params.requested_format.frame_size.height() < kMinFrameHeight) {
    DVLOG(1) << "invalid frame size: "
             << params.requested_format.frame_size.ToString();
    client->OnError();
    return;
  }

  base::TimeDelta capture_period = base::TimeDelta::FromMicroseconds(
      1000000.0 / params.requested_format.frame_rate + 0.5);

  scoped_ptr<VideoCaptureOracle> oracle(
      new VideoCaptureOracle(capture_period,
                             kAcceleratedSubscriberIsSupported));
  oracle_proxy_ =
      new ThreadSafeCaptureOracle(client.Pass(), oracle.Pass(), params);

  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&VideoCaptureMachine::Start,
                 base::Unretained(capture_machine_.get()),
                 oracle_proxy_),
      base::Bind(&VideoCaptureDeviceImpl::CaptureStarted,
                 AsWeakPtr()));

  TransitionStateTo(kCapturing);
}

void VideoCaptureDeviceImpl::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kCapturing)
    return;

  oracle_proxy_->Stop();
  oracle_proxy_ = NULL;

  TransitionStateTo(kIdle);

  // Stops the capture machine asynchronously.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &VideoCaptureMachine::Stop,
          base::Unretained(capture_machine_.get())));
}

void VideoCaptureDeviceImpl::CaptureStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!success) {
    DVLOG(1) << "Failed to start capture machine.";
    Error();
  }
}

VideoCaptureDeviceImpl::VideoCaptureDeviceImpl(
    scoped_ptr<VideoCaptureMachine> capture_machine)
    : state_(kIdle),
      capture_machine_(capture_machine.Pass()) {}

VideoCaptureDeviceImpl::~VideoCaptureDeviceImpl() {
  // If capture_machine is not NULL, then we need to return to the UI thread to
  // safely stop the capture machine.
  if (capture_machine_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(
            &DeleteCaptureMachineOnUIThread, base::Passed(&capture_machine_)));
  }
  DVLOG(1) << "VideoCaptureDeviceImpl@" << this << " destroying.";
}

void VideoCaptureDeviceImpl::TransitionStateTo(State next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

#ifndef NDEBUG
  static const char* kStateNames[] = {
    "Idle", "Allocated", "Capturing", "Error"
  };
  DVLOG(1) << "State change: " << kStateNames[state_]
           << " --> " << kStateNames[next_state];
#endif

  state_ = next_state;
}

void VideoCaptureDeviceImpl::Error() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kIdle)
    return;

  if (oracle_proxy_)
    oracle_proxy_->ReportError();

  StopAndDeAllocate();
  TransitionStateTo(kError);
}

}  // namespace content
