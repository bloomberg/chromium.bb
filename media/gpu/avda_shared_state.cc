// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_shared_state.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "media/gpu/avda_codec_image.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_make_current.h"

namespace media {

// Handle OnFrameAvailable callbacks safely.  Since they occur asynchronously,
// we take care that the object that wants them still exists.  WeakPtrs cannot
// be used because OnFrameAvailable callbacks can occur on any thread. We also
// can't guarantee when the SurfaceTexture will quit sending callbacks to
// coordinate with the destruction of the AVDA and PictureBufferManager, so we
// have a separate object that the callback can own.
class AVDASharedState::OnFrameAvailableHandler
    : public base::RefCountedThreadSafe<OnFrameAvailableHandler> {
 public:
  // We do not retain ownership of |listener|.  It must remain valid until after
  // ClearListener() is called.  This will register with |surface_texture| to
  // receive OnFrameAvailable callbacks.
  OnFrameAvailableHandler(AVDASharedState* listener,
                          gl::SurfaceTexture* surface_texture)
      : listener_(listener) {
    surface_texture->SetFrameAvailableCallbackOnAnyThread(
        base::Bind(&OnFrameAvailableHandler::OnFrameAvailable,
                   scoped_refptr<OnFrameAvailableHandler>(this)));
  }

  // Forget about |listener_|, which is required before one deletes it.
  // No further callbacks will happen once this completes.
  void ClearListener() {
    base::AutoLock lock(lock_);
    listener_ = nullptr;
  }

  // Notify the listener if there is one.
  void OnFrameAvailable() {
    base::AutoLock auto_lock(lock_);
    if (listener_)
      listener_->SignalFrameAvailable();
  }

 private:
  friend class base::RefCountedThreadSafe<OnFrameAvailableHandler>;

  ~OnFrameAvailableHandler() { DCHECK(!listener_); }

  // Protects changes to listener_.
  base::Lock lock_;

  // The AVDASharedState that wants the OnFrameAvailable callback.
  AVDASharedState* listener_;

  DISALLOW_COPY_AND_ASSIGN(OnFrameAvailableHandler);
};

AVDASharedState::AVDASharedState(
    scoped_refptr<AVDASurfaceBundle> surface_bundle)
    : frame_available_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),

      gl_matrix_{
          1, 0, 0, 0,  // Default to a sane guess just in case we can't get the
          0, 1, 0, 0,  // matrix on the first call. Will be Y-flipped later.
          0, 0, 1, 0,  //
          0, 0, 0, 1,  // Comment preserves 4x4 formatting.
      },
      surface_bundle_(surface_bundle),
      weak_this_factory_(this) {
  // If the surface bundle has a surface texture, then register to receive
  // OnFrameAvailable notifications.
  if (surface_texture()) {
    on_frame_available_handler_ =
        new OnFrameAvailableHandler(this, surface_texture());
  }

  // If we're holding a reference to an overlay, then register to drop it if the
  // overlay's surface is destroyed.
  if (overlay()) {
    overlay()->AddSurfaceDestroyedCallback(base::Bind(
        &AVDASharedState::OnSurfaceDestroyed, weak_this_factory_.GetWeakPtr()));
  }
}

AVDASharedState::~AVDASharedState() {
  if (on_frame_available_handler_)
    on_frame_available_handler_->ClearListener();
}

void AVDASharedState::SignalFrameAvailable() {
  frame_available_event_.Signal();
}

void AVDASharedState::WaitForFrameAvailable() {
  DCHECK(!release_time_.is_null());

  // 5msec covers >99.9% of cases, so just wait for up to that much before
  // giving up.  If an error occurs, we might not ever get a notification.
  const base::TimeDelta max_wait = base::TimeDelta::FromMilliseconds(5);
  const base::TimeTicks call_time = base::TimeTicks::Now();
  const base::TimeDelta elapsed = call_time - release_time_;
  const base::TimeDelta remaining = max_wait - elapsed;
  release_time_ = base::TimeTicks();

  if (remaining <= base::TimeDelta()) {
    if (!frame_available_event_.IsSignaled()) {
      DVLOG(1) << "Deferred WaitForFrameAvailable() timed out, elapsed: "
               << elapsed.InMillisecondsF() << "ms";
    }
    return;
  }

  DCHECK_LE(remaining, max_wait);
  SCOPED_UMA_HISTOGRAM_TIMER("Media.AvdaCodecImage.WaitTimeForFrame");
  if (!frame_available_event_.TimedWait(remaining)) {
    DVLOG(1) << "WaitForFrameAvailable() timed out, elapsed: "
             << elapsed.InMillisecondsF()
             << "ms, additionally waited: " << remaining.InMillisecondsF()
             << "ms, total: " << (elapsed + remaining).InMillisecondsF()
             << "ms";
  }
}

void AVDASharedState::RenderCodecBufferToSurfaceTexture(
    MediaCodecBridge* codec,
    int codec_buffer_index) {
  if (!release_time_.is_null())
    WaitForFrameAvailable();
  codec->ReleaseOutputBuffer(codec_buffer_index, true);
  release_time_ = base::TimeTicks::Now();
}

void AVDASharedState::UpdateTexImage() {
  surface_texture()->UpdateTexImage();
  // Helpfully, this is already column major.
  surface_texture()->GetTransformMatrix(gl_matrix_);
}

void AVDASharedState::GetTransformMatrix(float matrix[16]) const {
  memcpy(matrix, gl_matrix_, sizeof(gl_matrix_));
}

void AVDASharedState::OnSurfaceDestroyed(AndroidOverlay* overlay_raw) {
  if (surface_bundle_ && overlay() == overlay_raw)
    surface_bundle_ = nullptr;
}

}  // namespace media
