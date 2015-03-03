// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/content_video_capture_device_core.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

namespace {

void DeleteCaptureMachineOnUIThread(
    scoped_ptr<VideoCaptureMachine> capture_machine) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  capture_machine.reset();
}

}  // namespace

ThreadSafeCaptureOracle::ThreadSafeCaptureOracle(
    scoped_ptr<media::VideoCaptureDevice::Client> client,
    const media::VideoCaptureParams& params)
    : client_(client.Pass()),
      oracle_(base::TimeDelta::FromMicroseconds(
          static_cast<int64>(1000000.0 / params.requested_format.frame_rate +
                             0.5 /* to round to nearest int */))),
      params_(params) {}

ThreadSafeCaptureOracle::~ThreadSafeCaptureOracle() {}

bool ThreadSafeCaptureOracle::ObserveEventAndDecideCapture(
    VideoCaptureOracle::Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time,
    scoped_refptr<media::VideoFrame>* storage,
    CaptureFrameCallback* callback) {
  // Grab the current time before waiting to acquire the |lock_|.
  const base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  base::AutoLock guard(lock_);

  if (!client_)
    return false;  // Capture is stopped.

  const media::VideoFrame::Format video_frame_format =
      params_.requested_format.pixel_format == media::PIXEL_FORMAT_TEXTURE ?
          media::VideoFrame::NATIVE_TEXTURE : media::VideoFrame::I420;

  if (capture_size_.IsEmpty())
    capture_size_ = max_frame_size();
  const gfx::Size visible_size = capture_size_;
  // Always round up the coded size to multiple of 16 pixels.
  // See http://crbug.com/402151.
  const gfx::Size coded_size((visible_size.width() + 15) & ~15,
                             (visible_size.height() + 15) & ~15);

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer =
      client_->ReserveOutputBuffer(video_frame_format, coded_size);
  const bool should_capture =
      oracle_.ObserveEventAndDecideCapture(event, damage_rect, event_time);
  const bool content_is_dirty =
      (event == VideoCaptureOracle::kCompositorUpdate ||
       event == VideoCaptureOracle::kSoftwarePaint);
  const char* event_name =
      (event == VideoCaptureOracle::kTimerPoll ? "poll" :
       (event == VideoCaptureOracle::kCompositorUpdate ? "gpu" :
       "paint"));

  // Consider the various reasons not to initiate a capture.
  if (should_capture && !output_buffer.get()) {
    TRACE_EVENT_INSTANT1("mirroring",
                         "PipelineLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger",
                         event_name);
    return false;
  } else if (!should_capture && output_buffer.get()) {
    if (content_is_dirty) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("mirroring", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD,
                           "trigger", event_name);
    }
    return false;
  } else if (!should_capture && !output_buffer.get()) {
    // We decided not to capture, but we wouldn't have been able to if we wanted
    // to because no output buffer was available.
    TRACE_EVENT_INSTANT1("mirroring", "NearlyPipelineLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger", event_name);
    return false;
  }
  int frame_number = oracle_.RecordCapture();
  TRACE_EVENT_ASYNC_BEGIN2("mirroring", "Capture", output_buffer.get(),
                           "frame_number", frame_number,
                           "trigger", event_name);
  // NATIVE_TEXTURE frames wrap a texture mailbox, which we don't have at the
  // moment.  We do not construct those frames.
  if (video_frame_format != media::VideoFrame::NATIVE_TEXTURE) {
    *storage = media::VideoFrame::WrapExternalPackedMemory(
        video_frame_format,
        coded_size,
        gfx::Rect(visible_size),
        visible_size,
        static_cast<uint8*>(output_buffer->data()),
        output_buffer->size(),
        base::SharedMemory::NULLHandle(),
        0,
        base::TimeDelta(),
        base::Closure());
    DCHECK(*storage);
  }
  *callback = base::Bind(&ThreadSafeCaptureOracle::DidCaptureFrame,
                         this,
                         frame_number,
                         output_buffer,
                         capture_begin_time);
  return true;
}

gfx::Size ThreadSafeCaptureOracle::GetCaptureSize() const {
  base::AutoLock guard(lock_);
  return capture_size_.IsEmpty() ? max_frame_size() : capture_size_;
}

void ThreadSafeCaptureOracle::UpdateCaptureSize(const gfx::Size& source_size) {
  base::AutoLock guard(lock_);

  // Update |capture_size_| based on |source_size| if either: 1) The resolution
  // change policy specifies fixed frame sizes and |capture_size_| has not yet
  // been set; or 2) The resolution change policy specifies dynamic frame
  // sizes.
  if (capture_size_.IsEmpty() || params_.resolution_change_policy ==
      media::RESOLUTION_POLICY_DYNAMIC_WITHIN_LIMIT) {
    capture_size_ = source_size;
    // The capture size should not exceed the maximum frame size.
    if (capture_size_.width() > max_frame_size().width() ||
        capture_size_.height() > max_frame_size().height()) {
      capture_size_ = media::ComputeLetterboxRegion(
          gfx::Rect(max_frame_size()), capture_size_).size();
    }
    // The capture size must be even and not less than the minimum frame size.
    capture_size_ = gfx::Size(
        std::max(kMinFrameWidth, MakeEven(capture_size_.width())),
        std::max(kMinFrameHeight, MakeEven(capture_size_.height())));
  }
}

void ThreadSafeCaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  client_.reset();
}

void ThreadSafeCaptureOracle::ReportError(const std::string& reason) {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnError(reason);
}

void ThreadSafeCaptureOracle::DidCaptureFrame(
    int frame_number,
    const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& buffer,
    base::TimeTicks capture_begin_time,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp,
    bool success) {
  base::AutoLock guard(lock_);
  TRACE_EVENT_ASYNC_END2("mirroring", "Capture", buffer.get(),
                         "success", success,
                         "timestamp", timestamp.ToInternalValue());

  if (!client_)
    return;  // Capture is stopped.

  if (success) {
    if (oracle_.CompleteCapture(frame_number, &timestamp)) {
      // TODO(miu): Use the locked-in frame rate from AnimatedContentSampler.
      frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                   params_.requested_format.frame_rate);
      frame->metadata()->SetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_BEGIN_TIME, capture_begin_time);
      frame->metadata()->SetTimeTicks(
          media::VideoFrameMetadata::CAPTURE_END_TIME, base::TimeTicks::Now());
      client_->OnIncomingCapturedVideoFrame(buffer, frame, timestamp);
    }
  }
}

void ContentVideoCaptureDeviceCore::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<media::VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (params.requested_format.frame_rate <= 0) {
    std::string error_msg("Invalid frame_rate: ");
    error_msg += base::DoubleToString(params.requested_format.frame_rate);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

  if (params.requested_format.pixel_format != media::PIXEL_FORMAT_I420 &&
      params.requested_format.pixel_format != media::PIXEL_FORMAT_TEXTURE) {
    std::string error_msg = base::StringPrintf(
        "unsupported format: %d", params.requested_format.pixel_format);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

   if (params.requested_format.frame_size.width() < kMinFrameWidth ||
       params.requested_format.frame_size.height() < kMinFrameHeight) {
     std::string error_msg =
         "invalid frame size: " + params.requested_format.frame_size.ToString();
     DVLOG(1) << error_msg;
     client->OnError(error_msg);
     return;
   }

  media::VideoCaptureParams new_params = params;
  // Frame dimensions must each be an even integer since the client wants (or
  // will convert to) YUV420.
  new_params.requested_format.frame_size.SetSize(
      MakeEven(params.requested_format.frame_size.width()),
      MakeEven(params.requested_format.frame_size.height()));

  oracle_proxy_ = new ThreadSafeCaptureOracle(client.Pass(), new_params);

  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&VideoCaptureMachine::Start,
                 base::Unretained(capture_machine_.get()),
                 oracle_proxy_,
                 new_params),
      base::Bind(&ContentVideoCaptureDeviceCore::CaptureStarted, AsWeakPtr()));

  TransitionStateTo(kCapturing);
}

void ContentVideoCaptureDeviceCore::StopAndDeAllocate() {
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
          base::Unretained(capture_machine_.get()),
          base::Bind(&base::DoNothing)));
}

void ContentVideoCaptureDeviceCore::CaptureStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!success) {
    std::string reason("Failed to start capture machine.");
    DVLOG(1) << reason;
    Error(reason);
  }
}

ContentVideoCaptureDeviceCore::ContentVideoCaptureDeviceCore(
    scoped_ptr<VideoCaptureMachine> capture_machine)
    : state_(kIdle),
      capture_machine_(capture_machine.Pass()) {
  DCHECK(capture_machine_.get());
}

ContentVideoCaptureDeviceCore::~ContentVideoCaptureDeviceCore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(state_, kCapturing);
  // If capture_machine is not NULL, then we need to return to the UI thread to
  // safely stop the capture machine.
  if (capture_machine_) {
    VideoCaptureMachine* capture_machine_ptr = capture_machine_.get();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&VideoCaptureMachine::Stop,
                   base::Unretained(capture_machine_ptr),
                   base::Bind(&DeleteCaptureMachineOnUIThread,
                              base::Passed(&capture_machine_))));
  }
  DVLOG(1) << "ContentVideoCaptureDeviceCore@" << this << " destroying.";
}

void ContentVideoCaptureDeviceCore::TransitionStateTo(State next_state) {
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

void ContentVideoCaptureDeviceCore::Error(const std::string& reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kIdle)
    return;

  if (oracle_proxy_.get())
    oracle_proxy_->ReportError(reason);

  StopAndDeAllocate();
  TransitionStateTo(kError);
}

}  // namespace content
