// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojo/video_capture_types.mojom.h"
#include "mojo/public/cpp/system/buffer.h"

namespace content {

namespace {

// Transfers ownership of an object to a std::unique_ptr with a custom deleter
// that ensures the object is destroyed on the UI BrowserThread.
template <typename T>
std::unique_ptr<T, BrowserThread::DeleteOnUIThread> RescopeToUIThread(
    std::unique_ptr<T>&& ptr) {
  return std::unique_ptr<T, BrowserThread::DeleteOnUIThread>(ptr.release());
}

// Sets up a mojo message pipe and requests the HostFrameSinkManager create a
// new capturer instance bound to it. Returns the client-side interface.
viz::mojom::FrameSinkVideoCapturerPtrInfo CreateCapturer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  viz::HostFrameSinkManager* const manager = GetHostFrameSinkManager();
  DCHECK(manager);
  viz::mojom::FrameSinkVideoCapturerPtr capturer;
  manager->CreateVideoCapturer(mojo::MakeRequest(&capturer));
  return capturer.PassInterface();
}

// Adapter for a VideoFrameReceiver to get access to the mojo SharedBufferHandle
// for a frame.
class HandleMover
    : public media::VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  explicit HandleMover(mojo::ScopedSharedBufferHandle handle)
      : handle_(std::move(handle)) {}
  ~HandleMover() final {}

  mojo::ScopedSharedBufferHandle GetHandleForInterProcessTransit(
      bool read_only) final {
    return std::move(handle_);
  }

  base::SharedMemoryHandle GetNonOwnedSharedMemoryHandleForLegacyIPC() final {
    NOTREACHED();
    return base::SharedMemoryHandle();
  }

  std::unique_ptr<media::VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      final {
    NOTREACHED();
    return nullptr;
  }

 private:
  mojo::ScopedSharedBufferHandle handle_;
};

// Adapter for a VideoFrameReceiver to notify once frame consumption is
// complete. VideoFrameReceiver requires owning an object that it will destroy
// once consumption is complete. This class adapts between that scheme and
// running a "done callback" to notify that consumption is complete.
class ScopedFrameDoneHelper
    : public base::ScopedClosureRunner,
      public media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  ScopedFrameDoneHelper(base::OnceClosure done_callback)
      : base::ScopedClosureRunner(std::move(done_callback)) {}
  ~ScopedFrameDoneHelper() final = default;
};

}  // namespace

FrameSinkVideoCaptureDevice::FrameSinkVideoCaptureDevice()
    : capturer_creator_(base::BindRepeating(&CreateCapturer)),
      binding_(this),
      cursor_renderer_(RescopeToUIThread(CursorRenderer::Create(
          CursorRenderer::CURSOR_DISPLAYED_ON_MOUSE_MOVEMENT))),
      weak_factory_(this) {
  DCHECK(cursor_renderer_);
}

FrameSinkVideoCaptureDevice::~FrameSinkVideoCaptureDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_) << "StopAndDeAllocate() was never called after start.";
}

void FrameSinkVideoCaptureDevice::AllocateAndStartWithReceiver(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoFrameReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.IsValid());
  DCHECK(receiver);

  // If the device has already ended on a fatal error, abort immediately.
  if (fatal_error_message_) {
    receiver->OnLog(*fatal_error_message_);
    receiver->OnError();
    return;
  }

  capture_params_ = params;
  DCHECK(!receiver_);
  receiver_ = std::move(receiver);

  // Set a callback that will be run whenever the mouse moves, to trampoline
  // back to the device thread and request a refresh frame so that the new mouse
  // cursor location can be drawn in a new video frame.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CursorRenderer::SetNeedsRedrawCallback,
                     base::Unretained(cursor_renderer_.get()),
                     media::BindToCurrentLoop(base::BindRepeating(
                         &FrameSinkVideoCaptureDevice::RequestRefreshFrame,
                         weak_factory_.GetWeakPtr()))));

  // Hop to the UI thread to request a Mojo connection to a new capturer
  // instance, and then hop back to the device thread to start using it.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE, base::BindOnce(capturer_creator_),
      base::BindOnce(&FrameSinkVideoCaptureDevice::OnCapturerCreated,
                     weak_factory_.GetWeakPtr()));
}

void FrameSinkVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  // FrameSinkVideoCaptureDevice does not use a
  // VideoCaptureDevice::Client. Instead, it provides frames to a
  // VideoFrameReceiver directly.
  NOTREACHED();
}

void FrameSinkVideoCaptureDevice::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_ && binding_.is_bound() && !suspend_requested_) {
    capturer_->RequestRefreshFrame();
  }
}

void FrameSinkVideoCaptureDevice::MaybeSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = true;
  MaybeStopConsuming();
}

void FrameSinkVideoCaptureDevice::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  suspend_requested_ = false;
  MaybeStartConsuming();
}

void FrameSinkVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Discontinue requesting extra video frames to render mouse cursor changes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CursorRenderer::SetNeedsRedrawCallback,
                     base::Unretained(cursor_renderer_.get()),
                     base::RepeatingClosure()));

  MaybeStopConsuming();
  capturer_.reset();
  if (receiver_) {
    receiver_.reset();
  }
}

void FrameSinkVideoCaptureDevice::OnUtilizationReport(int frame_feedback_id,
                                                      double utilization) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Assumption: The "slot" should be valid at this point because this method
  // will always be called before the VideoFrameReceiver signals it is done
  // consuming the frame.
  const auto slot_index = static_cast<size_t>(frame_feedback_id);
  DCHECK_LT(slot_index, slots_.size());
  slots_[slot_index].callbacks->ProvideFeedback(utilization);
}

void FrameSinkVideoCaptureDevice::OnFrameCaptured(
    mojo::ScopedSharedBufferHandle buffer,
    uint32_t buffer_size,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& update_rect,
    const gfx::Rect& content_rect,
    viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callbacks);

  if (!receiver_ || !buffer.is_valid()) {
    callbacks->Done();
    return;
  }

  // Search for the next available ConsumptionState slot and bind |callbacks|
  // there.
  size_t slot_index = 0;
  for (;; ++slot_index) {
    if (slot_index == slots_.size()) {
      // The growth of |slots_| should be bounded because the
      // viz::mojom::FrameSinkVideoCapturer should enforce an upper-bound on the
      // number of frames in-flight.
      constexpr size_t kMaxInFlightFrames = 32;  // Arbitrarily-chosen limit.
      DCHECK_LT(slots_.size(), kMaxInFlightFrames);
      slots_.emplace_back();
      break;
    }
    if (!slots_[slot_index].callbacks.is_bound()) {
      break;
    }
  }
  ConsumptionState& slot = slots_[slot_index];
  slot.callbacks = std::move(callbacks);

  // Render the mouse cursor on the video frame, but first map the shared memory
  // into the current process in order to render the cursor.
  mojo::ScopedSharedBufferMapping mapping = buffer->Map(buffer_size);
  scoped_refptr<media::VideoFrame> frame;
  if (mapping) {
    frame = media::VideoFrame::WrapExternalData(
        info->pixel_format, info->coded_size, info->visible_rect,
        info->visible_rect.size(), static_cast<uint8_t*>(mapping.get()),
        buffer_size, info->timestamp);
    if (frame) {
      frame->AddDestructionObserver(base::BindOnce(
          [](mojo::ScopedSharedBufferMapping mapping) {}, std::move(mapping)));
      if (!cursor_renderer_->RenderOnVideoFrame(frame.get(), content_rect,
                                                &slot.undoer)) {
        // Release |frame| now, since no "undo cursor rendering" will be needed.
        frame = nullptr;
      }
    }
  }

  // Set the INTERACTIVE_CONTENT frame metadata.
  media::VideoFrameMetadata modified_metadata;
  if (info->metadata) {
    modified_metadata.MergeInternalValuesFrom(*info->metadata);
  }
  modified_metadata.SetBoolean(media::VideoFrameMetadata::INTERACTIVE_CONTENT,
                               cursor_renderer_->IsUserInteractingWithView());
  info->metadata = modified_metadata.CopyInternalValues();

  // Pass the video frame to the VideoFrameReceiver. This is done by first
  // passing the shared memory buffer handle and then notifying it that a new
  // frame is ready to be read from the buffer.
  receiver_->OnNewBufferHandle(
      static_cast<BufferId>(slot_index),
      std::make_unique<HandleMover>(std::move(buffer)));
  receiver_->OnFrameReadyInBuffer(
      static_cast<BufferId>(slot_index), slot_index,
      std::make_unique<ScopedFrameDoneHelper>(
          media::BindToCurrentLoop(base::BindOnce(
              &FrameSinkVideoCaptureDevice::OnFramePropagationComplete,
              weak_factory_.GetWeakPtr(), slot_index, std::move(frame)))),
      std::move(info));
}

void FrameSinkVideoCaptureDevice::OnTargetLost(
    const viz::FrameSinkId& frame_sink_id) {
  // This is ignored because FrameSinkVideoCaptureDevice subclasses always call
  // OnTargetChanged() and OnTargetPermanentlyLost() to resolve lost targets.
}

void FrameSinkVideoCaptureDevice::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This method would never be called if FrameSinkVideoCaptureDevice explicitly
  // called capturer_->Stop(), because the binding is closed at that time.
  // Therefore, a call to this method means that the capturer cannot continue;
  // and that's a permanent failure.
  OnFatalError("Capturer service cannot continue.");
}

void FrameSinkVideoCaptureDevice::OnTargetChanged(
    const viz::FrameSinkId& frame_sink_id,
    gfx::NativeView native_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  target_ = frame_sink_id;
  if (capturer_) {
    capturer_->ChangeTarget(frame_sink_id);
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CursorRenderer::SetTargetView,
                     base::Unretained(cursor_renderer_.get()), native_view));
}

void FrameSinkVideoCaptureDevice::OnTargetPermanentlyLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  target_ = viz::FrameSinkId();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&CursorRenderer::SetTargetView,
                     base::Unretained(cursor_renderer_.get()),
                     gfx::NativeView()));

  OnFatalError("Capture target has been permanently lost.");
}

void FrameSinkVideoCaptureDevice::SetCapturerCreatorForTesting(
    CapturerCreatorCallback creator) {
  capturer_creator_ = std::move(creator);
}

void FrameSinkVideoCaptureDevice::OnCapturerCreated(
    viz::mojom::FrameSinkVideoCapturerPtrInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_) {
    return;  // StopAndDeAllocate() occurred in the meantime.
  }

  // Shutdown the prior capturer, if any.
  MaybeStopConsuming();
  capturer_.reset();

  // Bind and configure the new capturer.
  capturer_.Bind(std::move(info));
  // TODO(miu): Remove this once HostFrameSinkManager will notify this
  // consumer when to take recovery steps after VIZ process crashes.
  capturer_.set_connection_error_handler(base::BindOnce(
      &FrameSinkVideoCaptureDevice::OnFatalError, base::Unretained(this),
      "Capturer service connection lost."));
  capturer_->SetFormat(capture_params_.requested_format.pixel_format,
                       media::COLOR_SPACE_UNSPECIFIED);
  capturer_->SetMinCapturePeriod(
      base::TimeDelta::FromMicroseconds(base::saturated_cast<int64_t>(
          base::Time::kMicrosecondsPerSecond /
          capture_params_.requested_format.frame_rate)));
  const auto& constraints = capture_params_.SuggestConstraints();
  capturer_->SetResolutionConstraints(constraints.min_frame_size,
                                      constraints.max_frame_size,
                                      constraints.fixed_aspect_ratio);

  if (target_.is_valid()) {
    capturer_->ChangeTarget(target_);
  }

  receiver_->OnStarted();

  if (!suspend_requested_) {
    MaybeStartConsuming();
  }
}

void FrameSinkVideoCaptureDevice::MaybeStartConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_ || !capturer_ || binding_.is_bound()) {
    return;
  }

  viz::mojom::FrameSinkVideoConsumerPtr consumer;
  binding_.Bind(mojo::MakeRequest(&consumer));
  // TODO(miu): Remove this once HostFrameSinkManager will notify this consumer
  // when to take recovery steps after VIZ process crashes.
  binding_.set_connection_error_handler(base::BindOnce(
      &FrameSinkVideoCaptureDevice::OnFatalError, base::Unretained(this),
      "Consumer connection to Capturer service lost."));
  capturer_->Start(std::move(consumer));
}

void FrameSinkVideoCaptureDevice::MaybeStopConsuming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capturer_) {
    capturer_->Stop();
  }
  binding_.Close();
}

void FrameSinkVideoCaptureDevice::OnFramePropagationComplete(
    size_t slot_index,
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LT(slot_index, slots_.size());

  // Notify the VideoFrameReceiver that the buffer is no longer valid.
  if (receiver_) {
    receiver_->OnBufferRetired(static_cast<BufferId>(slot_index));
  }

  // Undo the mouse cursor rendering, if any.
  ConsumptionState& slot = slots_[slot_index];
  if (frame) {
    slot.undoer.Undo(frame.get());
    frame = nullptr;
  }

  // Notify the capturer that consumption of the frame is complete.
  slot.callbacks->Done();
  slot.callbacks.reset();
}

void FrameSinkVideoCaptureDevice::OnFatalError(std::string message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fatal_error_message_ = std::move(message);
  if (receiver_) {
    receiver_->OnLog(*fatal_error_message_);
    receiver_->OnError();
  }

  StopAndDeAllocate();
}

FrameSinkVideoCaptureDevice::ConsumptionState::ConsumptionState() = default;
FrameSinkVideoCaptureDevice::ConsumptionState::~ConsumptionState() = default;
FrameSinkVideoCaptureDevice::ConsumptionState::ConsumptionState(
    FrameSinkVideoCaptureDevice::ConsumptionState&& other) = default;
FrameSinkVideoCaptureDevice::ConsumptionState&
FrameSinkVideoCaptureDevice::ConsumptionState::operator=(
    FrameSinkVideoCaptureDevice::ConsumptionState&& other) = default;

}  // namespace content
